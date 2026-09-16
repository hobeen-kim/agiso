#pragma once

#include <cstdint>
#include <optional>

struct IsobusId {
  std::uint8_t priority;
  std::uint32_t pgn;
  std::uint8_t source;
  std::optional<std::uint8_t> destination;
};

IsobusId decode_id(std::uint32_t can_id);

std::uint32_t encode_id(const IsobusId& id);
