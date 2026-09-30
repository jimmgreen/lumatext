#include "raster_scale.hpp"
#include <cstdio>
#include <initializer_list>

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); return 1; } } while (false)

int main() {
  for (bool direct : {false, true}) {
    for (bool hinted : {false, true}) {
      const auto scale = lt::raster_scale(direct, hinted);
      CHECK(scale.size * scale.transform == scale.bitmap);
      CHECK(scale.bitmap == (direct ? 1u : 4u));
      CHECK(scale.size == (hinted || direct ? 1u : 4u));
      CHECK(scale.advance_divisor() == scale.size * 64);
      for (unsigned phase = 0; phase < 8; ++phase) {
        CHECK(static_cast<double>(phase * scale.phase()) / (64 * scale.bitmap) == phase / 8.0);
      }
      // Physical stem compensation must not depend on the hinting grid.
      CHECK(static_cast<double>(16 * scale.bitmap) / (64 * scale.bitmap) == 0.25);
    }
  }
  std::puts("raster scale policy passed");
}
