#include "address_claim.hpp"
#include "isobus_id.hpp"

#include <stdexcept>

CanFrame make_address_claim(
  const IsobusName& name,
  std::uint8_t source_address
)
{
  if (source_address >= 0xFE) {
    throw std::invalid_argument("Claimed source address must be 0x00..0xFD");
  }

  const std::uint64_t encoded_name = encode_name(name);

  IsobusId id{};
  id.priority = 6;
  id.pgn = PGN_ADDRESS_CLAIM;
  id.source = source_address;
  id.destination = ISOBUS_GLOBAL_ADDRESS;

  CanFrame frame{};
  frame.format = CanIdFormat::Extended29;
  frame.can_id = encode_id(id);
  frame.length = 8;

  //NAME 을 하위 바이트부터 작성
  for (unsigned i = 0; i < frame.length; ++i) {
    frame.data[i] = static_cast<std::uint8_t>(
      (encoded_name >> (8 * i)) & 0xFF
    );
  }

  return frame;
}

std::optional<IsobusName> parse_address_claim(
  const CanFrame& frame
)
{
  // ISOBUS 메시지는 29비트 확장임
  if (frame.format != CanIdFormat::Extended29) {
    return std::nullopt;
  }

  if (frame.can_id > CAN_ID_MAX) {
    return std::nullopt;
  }

  //NAME 은 정확히 8 바이트다.
  if (frame.length != 8) {
    return std::nullopt;
  }

  const IsobusId id = decode_id(frame.can_id);

  // Address Claim 인지 확인
  if (id.pgn != PGN_ADDRESS_CLAIM) {
    return std::nullopt;
  }

  // Address Claim destination 이 맞는지 확인 
  if (id.destination != ISOBUS_GLOBAL_ADDRESS) {
    return std::nullopt;
  }

  // 전체 목적지 주소를 송신 주소로 사용할 수 없다.
  if (id.source == ISOBUS_GLOBAL_ADDRESS) {
    return std::nullopt;
  }

  std::uint64_t encoded_name = 0;

  for (unsigned i = 0; i < frame.length; ++i) {
    encoded_name |= static_cast<std::uint64_t>(frame.data[i]) << (8 * i);
  }

  return decode_name(encoded_name);
}
