#include "can_frame.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>

void validate_frame(const CanFrame& frame)
{
  swtich (frame.format) {
    case CanIdFormat::Standard11:
      if (frame.can_id > CAN_STANDARD_ID_MAX) {
        throw std::invalid_argument("Standard CAN ID exceeds 11 bits");
      }
      break;

    case CanIdFormat::Extended29:
      if (frame.can_id > CAN_ID_MAX) {
        throw std::invalid_argument("Extended CAN ID exceeds 29 bits");
      }
      break;

    default:
      throw std::invalid_argument("Unknown CAN ID format");
  }

  if(frame.length > frame.data.size()) {
    throw std::invalid_argument("Classic CAN allows up to 8 bytes");
  }
}

void print_frame(const CanFrame& frame)
{
  const bool extended =
          frame.format == CanIdFormat::Extended29;

  std::cout
      << (extended ? "EXT " : "STD ")
      << "ID=0x"
      << std::hex << std::uppercase
      << std::setfill('0')
      << std::setw(extended ? 8 : 3)
      << frame.can_id
      << std::dec
      << " LEN=" << static_cast<unsigned>(frame.length)
      << " DATA=";

  for (unsigned i = 0; i < frame.length; ++i) {
      std::cout
          << std::hex
          << std::setw(2)
          << static_cast<unsigned>(frame.data[i])
          << ' ';
  }

  std::cout
      << std::dec
      << std::setfill(' ')
      << std::endl;
}