#pragma once

#include <cstdint>

namespace lt {

// DirectWrite packs the first ISO 15924 character in the low byte; HarfBuzz
// HB_TAG packs it in the high byte. Convert the value, not host memory order.
constexpr uint32_t dwrite_script_tag_to_hb(uint32_t tag) noexcept {
  return ((tag & 0x000000ffu) << 24) | ((tag & 0x0000ff00u) << 8) |
         ((tag & 0x00ff0000u) >> 8) | ((tag & 0xff000000u) >> 24);
}

}  // namespace lt
