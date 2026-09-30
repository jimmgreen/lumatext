#include "script_tag.hpp"

#include <cstdio>

// Values match DWRITE_MAKE_OPENTYPE_TAG inputs and HarfBuzz HB_TAG outputs.
static_assert(lt::dwrite_script_tag_to_hb(0x6e74614cu) == 0x4c61746eu);  // Latn
static_assert(lt::dwrite_script_tag_to_hb(0x62617241u) == 0x41726162u);  // Arab
static_assert(lt::dwrite_script_tag_to_hb(0x72626548u) == 0x48656272u);  // Hebr
static_assert(lt::dwrite_script_tag_to_hb(0x696e6148u) == 0x48616e69u);  // Hani
static_assert(lt::dwrite_script_tag_to_hb(0x7979795au) == 0x5a797979u);  // Zyyy
static_assert(lt::dwrite_script_tag_to_hb(0x686e695au) == 0x5a696e68u);  // Zinh
static_assert(lt::dwrite_script_tag_to_hb(0u) == 0u);

int main() {
  std::puts("DirectWrite-to-HarfBuzz script tag conversion passed");
  return 0;
}
