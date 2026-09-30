#include <ft2build.h>
#include FT_FREETYPE_H
#include "raster_scale.hpp"
#include <cstdio>
#include <initializer_list>
int main(int argc, char** argv) {
  // Supply a local outline font; no font files are redistributed by this test.
  if(argc != 2) { std::fprintf(stderr, "usage: raster_hinting_test FONT_PATH\n"); return 1; }
  FT_Library lib{}; FT_Face face{};
  if(FT_Init_FreeType(&lib) || FT_New_Face(lib, argv[1], 0, &face)) return 2;
  int checks=0, old_differences=0;
  for(unsigned dpi : {96,120,144,192}) for(unsigned size : {10,13,16}) for(unsigned c=33;c<127;++c) {
    FT_Pos native=0;
    for(int mode=0;mode<3;++mode) {
      const bool direct=mode==0;
      const auto s=lt::raster_scale(direct, true);
      unsigned size_scale=mode==2?4:s.size;
      if(FT_Set_Char_Size(face,0,size*48,dpi*size_scale,dpi*size_scale)) return 3;
      FT_Fixed matrix_scale=mode==2?65536:s.transform*65536;
      FT_Matrix matrix{matrix_scale,0,0,matrix_scale};
      FT_Set_Transform(face,&matrix,nullptr);
      if(FT_Load_Char(face,c,FT_LOAD_DEFAULT|FT_LOAD_NO_BITMAP)) return 4;
      const FT_Pos advance=face->glyph->metrics.horiAdvance;
      if(mode==0) native=advance;
      if(mode==1 && native!=advance) return 5;
      if(mode==2 && native*4!=advance) ++old_differences;
      if(FT_Render_Glyph(face->glyph,FT_RENDER_MODE_NORMAL)) return 6;
      ++checks;
    }
  }
  int major,minor,patch; FT_Library_Version(lib,&major,&minor,&patch);
  printf("FreeType %d.%d.%d: %d glyph probes; corrected advances agree; old 4x hint differed on %d cases\n",major,minor,patch,checks,old_differences);
  FT_Done_Face(face); FT_Done_FreeType(lib); return 0;
}
