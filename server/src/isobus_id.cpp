#include "isobus_id.hpp"
#include "can_frame.hpp"

#include <stdexcept>

IsobusId decode_id(std::uint32_t can_id)
{
  if (can_id > CAN_ID_MAX) {
    throw std::invalid_argument("CAN ID exceeds 29 bits");
  }

  const auto priority = static_cast<std::uint8_t>((can_id >> 26) & 0x07);
  const auto pf = static_cast<std::uint8_t>((can_id >> 16) & 0xFF);
  const auto ps = static_cast<std::uint8_t>((can_id >> 8) & 0xFF);
  const auto source = static_cast<std::uint8_t>(can_id & 0xFF);

  auto pgn = (can_id >> 8) & 0x3FFFF;
  std::optional<std::uint8_t> destination;

  if (pf < 240) {
    pgn &= 0x3FF00;
    destination = ps;
  }

  return {priority, pgn, source, destination};
}

std::uint32_t encode_id(const IsobusId& id)
  {
      if (id.priority > 7) {
          throw std::invalid_argument("Priority must be 0..7");
      }

      if (id.pgn > 0x3FFFF) {
          throw std::invalid_argument("PGN exceeds 18 bits");
      }

      const auto pf = (id.pgn >> 8) & 0xFF;

      std::uint32_t can_id =
          (static_cast<std::uint32_t>(id.priority) << 26) |
          (id.pgn << 8) |
          id.source;

      if (pf < 240) {
          if ((id.pgn & 0xFF) != 0) {
              throw std::invalid_argument(
                  "PDU1 PGN must have its low byte cleared");
          }

          if (!id.destination.has_value()) {
              throw std::invalid_argument(
                  "PDU1 requires a destination; use 0xFF for global");
          }

          can_id |=
              static_cast<std::uint32_t>(*id.destination) << 8;
      } else if (id.destination.has_value()) {
          throw std::invalid_argument(
              "PDU2 has no separate destination field");
      }

      return can_id;
  }
