add_executable(lumatext_abi_test abi_test.cpp)
target_link_libraries(lumatext_abi_test PRIVATE LumaText::Core)
target_compile_definitions(lumatext_abi_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME abi COMMAND lumatext_abi_test)

add_executable(lumatext_c_header_test c_header_test.c)
target_link_libraries(lumatext_c_header_test PRIVATE LumaText::Core)
target_compile_definitions(lumatext_c_header_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME c_header COMMAND lumatext_c_header_test)

add_executable(lumatext_render_smoke render_smoke.cpp)
target_link_libraries(lumatext_render_smoke PRIVATE LumaText::Core d2d1 dwrite windowscodecs ole32)
target_compile_definitions(lumatext_render_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(lumatext_render_smoke PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME render_smoke COMMAND lumatext_render_smoke)

add_executable(lumatext_coretext_layout_test coretext_layout_test.cpp)
target_link_libraries(lumatext_coretext_layout_test PRIVATE
  LumaText::Core d2d1 windowscodecs ole32)
target_compile_definitions(lumatext_coretext_layout_test PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(lumatext_coretext_layout_test PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME coretext_layout COMMAND lumatext_coretext_layout_test)

add_executable(lumatext_color_glyph_test color_glyph_test.cpp)
target_link_libraries(lumatext_color_glyph_test PRIVATE
  LumaText::Core d2d1 windowscodecs ole32)
target_compile_definitions(lumatext_color_glyph_test PRIVATE
  NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(lumatext_color_glyph_test PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME color_glyph COMMAND lumatext_color_glyph_test)

if(TARGET lumatext_static)
  add_executable(lumatext_rasterizer_bounds_test rasterizer_bounds_test.cpp)
  target_include_directories(lumatext_rasterizer_bounds_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    "${harfbuzz_SOURCE_DIR}/src"
    "${icu4c_SOURCE_DIR}/include"
    "${freetype_SOURCE_DIR}/include")
  target_link_libraries(lumatext_rasterizer_bounds_test PRIVATE
    lumatext_static d2d1 windowscodecs ole32)
  target_compile_definitions(lumatext_rasterizer_bounds_test PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE HB_STATIC)
  target_compile_options(lumatext_rasterizer_bounds_test PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
  add_test(NAME rasterizer_bounds COMMAND lumatext_rasterizer_bounds_test)
endif()

lumatext_stage_runtime_dependencies(lumatext_abi_test)
lumatext_stage_runtime_dependencies(lumatext_c_header_test)
lumatext_stage_runtime_dependencies(lumatext_render_smoke)
lumatext_stage_runtime_dependencies(lumatext_coretext_layout_test)
lumatext_stage_runtime_dependencies(lumatext_color_glyph_test)
if(TARGET lumatext_rasterizer_bounds_test)
  lumatext_stage_runtime_dependencies(lumatext_rasterizer_bounds_test)
endif()
