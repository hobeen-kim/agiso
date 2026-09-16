#pragma once

#include "can_frame.hpp"
#include "isobus_name.hpp"

#include <cstdint>
#include <optional>

constexpr std::uint32_t PGN_ADDRESS_CLAIM = 0xEE00;
constexpr std::uint8_t ISOBUS_GLOBAL_ADDRESS = 0xFF;

CanFrame make_address_claim(
  const IsobusName& name,
  std::uint8_t source_address
);

std::optional<IsobusName> parse_address_claim(
  const CanFrame& frame
);
