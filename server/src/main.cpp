#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <iomanip>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include "can_frame.hpp"
#include "isobus_id.hpp"
#include "socketcan.hpp"

#include "address_claim.hpp"
#include "isobus_name.hpp"
#include <poll.h>

int main()
{
  const char* interface_name = "vcan0";

  // 1. CAN 프레임을 직접 송수신할 소켓 생성
  const int socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW); // 내가 만든 소켓의 번호

  if(socket_fd < 0) {
    std::perror("socket");
    return 1;
  }

  // 2. vcan0 이름의 인터페이스 조회
  const unsigned interface_index = if_nametoindex(interface_name); // 운영체제에 있는 can 인터페이스의 번호

  if (interface_index == 0) {
    std::perror("if_nametoindex");
    close(socket_fd);
    return 1;
  }

  // 3. socket 과 vcan0 연결
  sockaddr_can address{};  //소켓에 CAN 인터페이스를 지정하기 위한 Linux 구조체
  address.can_family = AF_CAN; //CAN 주소 형식을 사용한다고 지정
  address.can_ifindex = static_cast<int>(interface_index); //앞에서 찾은 인터페이스 번호 지정.

  if(bind( // 소켓과 vcan 을 묶음
      socket_fd,
      reinterpret_cast<const sockaddr*>(&address),
      sizeof(address)) < 0) {
        std::perror("bind");
        close(socket_fd);
        return 1;
    }

  try {

    IsobusName my_name{};
    my_name.identity_number = 1;
    my_name.manufacturer_code = 0;  // 실험·개발용
    my_name.ecu_instance = 0;
    my_name.function_instance = 0;
    my_name.function_code = 29;     // VT
    my_name.device_class = 0;
    my_name.device_class_instance = 0;
    my_name.industry_group = 2;     // 농업·임업
    my_name.arbitrary_address_capable = false;

    // 사용하려는 송신 주소
    constexpr std::uint8_t preferred_address = 0x80;

    // Address Claim 프레임 생성
    const CanFrame claim = make_address_claim(my_name, preferred_address);

    if (!send_frame(socket_fd, claim)) {
        close(socket_fd);
        return 1;
    }

    std::cout << "TX Address Claim: ";
    print_frame(claim);

  } catch (const std::exception& error) {
      std::cerr << "Address Claim send failed: " << error.what() << '\n';
      close(socket_fd);
      return 1;
  }

  std::cout
    << "Listening on " << interface_name
    << " ... Ctrl+C to stop"
    << std::endl;

  const auto started_at = std::chrono::steady_clock::now();

  //시험용 송신 주기: 1초
  const auto send_interval = std::chrono::milliseconds(1000);

  //첫 송신 예정 시각
  auto next_send_at = started_at + send_interval;

  //송신횟수를 구분할 값 (0~255)
  std::uint8_t counter = 0;

  //주기적으로 보낼 시험용 프레임
  CanFrame periodic_frame{};
  periodic_frame.format = CanIdFormat::Standard11;
  periodic_frame.can_id = 0x321;
  periodic_frame.length = 1;

  // 계속 수신
  while (true) {

    //매 반복마다 현재시간 확인
    const auto now = std::chrono::steady_clock::now();

    if(now >= next_send_at) {
      periodic_frame.data[0] = counter;

      if(!send_frame(socket_fd, periodic_frame)) {
        close(socket_fd);
        return 1;
      }

      std::cout << "TX ";
      print_frame(periodic_frame);

      ++counter;
      next_send_at = now + send_interval;
    }

    pollfd event{};
    event.fd = socket_fd;
    event.events = POLLIN;

    // 읽을 메시지가 있거나 100ms 지나면 반환
    const int ready = poll(&event, 1, 100);

    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }

      std::perror("poll");
      close(socket_fd);
      return 1;
    }

    //수신된 메시지 없음
    if (ready == 0) {
      continue;
    }

    // 소켓 오류나 연결 종료 상태 확인
     if (event.revents & (POLLERR | POLLHUP | POLLNVAL)) {
         std::cerr << "CAN socket error or hangup\n";
         close(socket_fd);
         return 1;
     }

     // 읽을 데이터가 있을 때만 아래 read() 실행
     if (!(event.revents & POLLIN)) {
         continue;
     }

    can_frame raw{}; //linux socketcan 이 사용하는 구조체.

    const auto received = read(socket_fd, &raw, sizeof(raw));

    if (received < 0) {
      if(errno == EINTR) {
        continue;
      }

      std::perror("read");
      close(socket_fd);
      return 1;
    }

    const auto received_at = std::chrono::steady_clock::now();

    if (received != static_cast<ssize_t>(sizeof(raw))) {
      std::cerr << "Unexpected CAN frame size\n";
      continue;
    }

    // 이번 프로그램에서는 일반 데이터 프레임만 처리
    if (raw.can_id & (CAN_ERR_FLAG | CAN_RTR_FLAG)) {
      continue;
    }

    if (raw.len > CAN_MAX_DLEN) {
      std::cerr << "Invalid CAN data length\n";
      continue;
    }

    const auto timestamp_us = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          received_at - started_at
      ).count()
    );

    const auto frame = to_can_frame(raw, timestamp_us);

    // 7. 수신 내용 출력
    print_frame(frame);

    // 확장 프레임을 J1939·ISOBUS 형식으로 해석
    if (frame.format == CanIdFormat::Extended29) {
      const auto id = decode_id(frame.can_id);

      std::cout
          << std::dec
          << "  priority    = "
          << static_cast<unsigned>(id.priority)
          << '\n'

          << std::hex << std::uppercase
          << "  pgn         = 0x" << id.pgn
          << '\n'

          << "  source      = 0x"
          << static_cast<unsigned>(id.source)
          << '\n';

      if (id.destination.has_value()) {
          std::cout
              << "  destination = 0x"
              << static_cast<unsigned>(*id.destination)
              << '\n';
      } else {
          std::cout << "  destination = none (PDU2)\n";
      }

      std::cout << std::dec << std::flush;

      const auto received_name = parse_address_claim(frame);

      if (received_name.has_value()) {
        if (received_name.has_value()) {
          if (id.source == 0xFE) {
            std::cout << " Cannot Claim Address received\n";
          } else {
            std::cout << " Address Claim received\n";
          }

           std::cout
              << "  identity_number   = "
              << received_name->identity_number << '\n'

              << "  manufacturer_code = "
              << received_name->manufacturer_code << '\n'

              << "  function_code     = "
              << static_cast<unsigned>(received_name->function_code)
              << '\n'

              << "  industry_group    = "
              << static_cast<unsigned>(received_name->industry_group)
              << '\n'

              << std::flush;
        }
      }
    }
  }
}
