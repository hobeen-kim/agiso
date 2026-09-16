#pragma once

#include <array>
#include <cstdint>

constexpr std::uint32_t CAN_STANDARD_ID_MAX = 0x7FF;
constexpr std::uint32_t CAN_ID_MAX = 0x1FFFFFFF;

enum class CanIdFormat {
  Standard11,
  Extended29
};

struct CanFrame {
  std::uint32_t can_id = 0;
  CanIdFormat format = CanIdFormat::Extended29;
  std::array<std::uint8_t, 8> data{};
  std::uint8_t length = 0;
  std::uint64_t timestamp_us = 0;
};

void validate_frame(const CanFrame& frame);
void print_frame(const CanFrame& frame);