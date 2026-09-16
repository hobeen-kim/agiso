#include "socketcan.hpp"

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <unistd.h>

CanFrame to_can_frame(
  const can_frame& raw,
  std::uint64_t timestamp_us
)
{
  const bool extended = (raw.can_id & CAN_EFF_FLAG) != 0;

  CanFrame frame{};

  frame.format = extended ? CanIdFormat::Extended29 : CanIdFormat::Standard11;

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

    // 확장 형식 표시 추가
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
