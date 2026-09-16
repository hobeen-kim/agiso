#pragma once

#include "can_frame.hpp"

#include <cstdint>
#include <linux/can.h>

CanFrame to_can_frame(
  const can_frame& raw,
  std::uint64_t timestamp_us
);

can_frame to_socketcan_frame(const CanFrame& frame);

bool send_frame(int socket_fd, const CanFrame& frame);