#pragma once

#include <cstdint>

struct IsobusName {
  // 제조사가 부여하는 식별번호 21비트
  std::uint32_t identity_number = 0;

  // 제조사 코드 11비트
  std::uint16_t manufacturer_code = 0;

  // 같은 기능 내 ECU 구분 번호 : 3비트
  std::uint8_t ecu_instance = 0;

  // 같은 종류의 기능을 구분하는 번호 : 5비트
  std::uint8_t function_instance = 0;

  // 기능 코드 : 8비트
  std::uint8_t function_code = 0;

  // 장치 종류 7비트
  std::uint8_t device_class = 0;

  // 같은 종류의 장치를 구분하는 번호 4비트
  std::uint8_t device_class_instance = 0;

  // 산업 분야 3비트
  std::uint8_t industry_group = 0;

  // 주소 충돌 시 다른 주소를 선택할 수 있는지
  bool arbitrary_address_capable = false;
};

std::uint64_t encode_name(const IsobusName& name);

IsobusName decode_name(std::uint64_t value);