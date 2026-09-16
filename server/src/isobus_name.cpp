#include "isobus_name.hpp"

#include <stdexcept>

std::uint64_t encode_name(const IsobusName& name)
{
  if (name.identity_number > 0x1FFFFF) {
      throw std::invalid_argument("Identity number exceeds 21 bits");
  }

  if (name.manufacturer_code > 0x7FF) {
    throw std::invalid_argument("Manufacturer code exceeds 11 bits");
  }

  if (name.ecu_instance > 0x07) {
    throw std::invalid_argument("ECU instance exceeds 3 bits");
  }

  if (name.function_instance > 0x1F) {
    throw std::invalid_argument("Function instance exceeds 5 bits");
  }

  if (name.device_class > 0x7F) {
    throw std::invalid_argument("Device class exceeds 7 bits");
  }

  if (name.device_class_instance > 0x0F) {
    throw std::invalid_argument("Device class instance exceeds 4 bits");
  }

  if (name.industry_group > 0x07) {
    throw std::invalid_argument("Industry group exceeds 3 bits");
  }

  // 0부터 시작하므로 값을 넣지 않는 예약 비트 48 은 0 으로 유지
  std::uint64_t value = 0;

  value |= static_cast<std::uint64_t>(name.identity_number);
  value |= static_cast<std::uint64_t>(name.manufacturer_code) << 21;
  value |= static_cast<std::uint64_t>(name.ecu_instance) << 32;
  value |= static_cast<std::uint64_t>(name.function_instance) << 35;
  value |= static_cast<std::uint64_t>(name.function_code) << 40;
  value |= static_cast<std::uint64_t>(name.device_class) << 49;
  value |= static_cast<std::uint64_t>(name.device_class_instance) << 56;
  value |= static_cast<std::uint64_t>(name.industry_group) << 60;
  value |= static_cast<std::uint64_t>(name.arbitrary_address_capable) << 63;

  return value;
}

IsobusName decode_name(std::uint64_t value)
{
  IsobusName name{};

  name.identity_number = static_cast<std::uint32_t>(value & 0x1FFFFF);
  name.manufacturer_code = static_cast<std::uint16_t>((value >> 21) & 0x7FF);
  name.ecu_instance = static_cast<std::uint8_t>((value >> 32) & 0x07);
  name.function_instance = static_cast<std::uint8_t>((value >> 35) & 0x1F);
  name.function_code = static_cast<std::uint8_t>((value >> 40) & 0xFF);
  // 예약 비트 48은 해석하지 않는다.
  name.device_class = static_cast<std::uint8_t>((value >> 49) & 0x7F);
  name.device_class_instance = static_cast<std::uint8_t>((value >> 56) & 0x0F);
  name.industry_group = static_cast<std::uint8_t>((value >> 60) & 0x07);
  name.arbitrary_address_capable = ((value >> 63) & 0x01) != 0;

  return name;
}