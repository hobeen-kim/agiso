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

struct IsobusId {
  std::uint8_t priority; //우선순위
  std::uint32_t pgn; //parameter group number, 메시지 종류
  std::uint8_t source; // 출발지
  std::optional<std::uint8_t> destination; //목적지
};

// Priority	3	메시지 우선순위. 0이 최고, 7이 최저
// EDP	1	Extended Data Page. J1939에서는 보통 0
// DP	1	Data Page. PGN 공간을 두 배로 확장
// PF	8	PDU Format. 메시지 유형 결정 (PDU1 vs PDU2 구분)
// PS	8	PDU Specific. PF < 240이면 목적지 주소, PF >= 240이면 그룹 확장
// SA	8	Source Address. 메시지를 보내는 노드의 주소 (0x00~0xFD)
IsobusId decode_id(std::uint32_t can_id)
{
  if (can_id > CAN_ID_MAX) {
    throw std::invalid_argument("CAN ID exceeds 29 bits");
  }

  const auto priority = static_cast<std::uint8_t>((can_id >> 26) & 0x07);
  
  const auto pf = static_cast<std::uint8_t>((can_id >> 16) & 0xFF); //PDU Format의 약자로, 메시지 형식을 구분하는 8비트 값. PGN의 일부로서 메시지 종류를 구분

  const auto ps = static_cast<std::uint8_t>((can_id >> 8) & 0xFF);

  const auto source = static_cast<std::uint8_t>(can_id & 0xFF);

  auto pgn = (can_id >> 8) & 0x3FFFF;
  std::optional<std::uint8_t> destination;

  if(pf < 240) { //PDU1 이라면
    pgn &= 0x3FF00; // 하위 8비트를 자르고
    destination = ps; //ps 를 목적지로 한다.
  }

  return {priority, pgn, source, destination};
}

std::uint32_t encode_id(const IsobusId& id)
{
  if (id.priority > 7) {
    throw std::invalid_argument("Priority must be 0..7");
  }

  if(id.pgn > 0x3FFFF) {
    throw std::invalid_argument("PGN exceeds 18 bits");
  }

  const auto pf = (id.pgn >> 8) & 0xFF;

  std::uint32_t can_id = 
    (static_cast<std::uint32_t>(id.priority) << 26) |
    (id.pgn << 8) |
    id.source;

  if (pf < 240) {
    if((id.pgn & 0xFF) != 0) {
      throw std::invalid_argument("PDU1 PGN must have its low byte cleared");
    }
    if(!id.destination.has_value()) {
      throw std::invalid_argument("PDU1 requires a destination; use 0xFF for global");
    }

    can_id |= static_cast<std::uint32_t>(*id.destination) << 8;
  } else if (id.destination.has_value()) {
    throw std::invalid_argument("PDU2 has no separate destination field");
  }

  return can_id;
}

// 호출 전에 프레임 종류와 데이터 길이를 검사한 SocketCAN 프레임을 변환합니다.
CanFrame to_can_frame(const can_frame& raw, std::uint64_t timestamp_us)
{
  const bool extended = (raw.can_id & CAN_EFF_FLAG) != 0;

  CanFrame frame{};
  frame.format = extended ? CanIdFormat::Extended29 : CanIdFormat::Standard11;
  // SocketCAN 플래그를 제거하고 순수 ID만 저장
  frame.can_id = raw.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);
  frame.length = raw.len;

  for (unsigned i = 0; i < frame.length; ++i) {
    frame.data[i] = raw.data[i];
  }

  frame.timestamp_us = timestamp_us;
  return frame;
}

can_frame to_socketcan_frame(const CanFrame& frame)
{
  validate_frame(frame);

  can_frame raw{};
  raw.can_id = frame.can_id;

  if (frame.format == CanIdFormat::Extended29) {
    raw.can_id |= CAN_EFF_FLAG;
  }

  raw.len = frame.length;

  for (unsigned i = 0; i < frame.length; ++i) {
    raw.data[i] = frame.data[i];
  }

  return raw;
}

bool send_frame(int socket_fd, const CanFrame& frame)
{
  const auto raw = to_socketcan_frame(frame);

  ssize_t sent;

  do {
    sent = write(socket_fd, &raw, sizeof(raw));
  } while (sent < 0 && errno == EINTR);

  if (sent < 0) {
    std::perror("write");
    return false;
  }

  if (sent != static_cast<ssize_t>(sizeof(raw))) {
    std::cerr << "Incomplete CAN frame write\n";
    return false;
  }

  return true;
}

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
    CanFrame standard{};
    standard.format = CanIdFormat::Standard11;
    standard.can_id = 0x123;
    standard.length = 4;
    standard.data = {0x11, 0x22, 0x33, 0x44};

    if (!send_frame(socket_fd, standard)) {
        close(socket_fd);
        return 1;
    }

    IsobusId id{};
    id.priority = 6;
    id.pgn = 0xEA00;
    id.source = 0x80;
    id.destination = 0x23;

    CanFrame extended{};
    extended.format = CanIdFormat::Extended29;
    extended.can_id = encode_id(id);
    extended.length = 3;
    extended.data = {0x00, 0xEE, 0x00};

    if (!send_frame(socket_fd, extended)) {
        close(socket_fd);
        return 1;
    }

    std::cout << "Sent 2 test frames\n";
  } catch (const std::exception& error) {
      std::cerr << "Send failed: " << error.what() << '\n';
      close(socket_fd);
      return 1;
  }

  std::cout
    << "Listening on " << interface_name
    << " ... Ctrl+C to stop"
    << std::endl;

  const auto started_at = std::chrono::steady_clock::now();

  // 계속 수신
  while (true) {
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
    }
  }
}
