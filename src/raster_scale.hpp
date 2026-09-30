#pragma once

namespace lt {

// Keep hinting in the destination pixel grid. Supersampling scales the hinted
// outline afterwards; unhinted rendering keeps its existing high-resolution size.
struct RasterScale {
  unsigned size;
  unsigned transform;
  unsigned bitmap;
  constexpr unsigned phase() const noexcept { return bitmap * 8; }
  constexpr unsigned advance_divisor() const noexcept { return size * 64; }
};

constexpr RasterScale raster_scale(bool direct, bool hinted) noexcept {
  const unsigned bitmap = direct ? 1u : 4u;
  const unsigned size = hinted ? 1u : bitmap;
  return {size, bitmap / size, bitmap};
}

}  // namespace lt
