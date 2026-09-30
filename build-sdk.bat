@echo off
setlocal
set "LT_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%LT_VSWHERE%" exit /b 1
for /f "usebackq delims=" %%i in (`"%LT_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "LT_VS=%%i"
if not defined LT_VS exit /b 1
call "%LT_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
chcp 65001 >nul
set "VSLANG=1033"
set "LT_HB_ARG="
set "LT_FT_ARG="
if exist "%~dp0build-min-nmake\_deps\harfbuzz-src\CMakeLists.txt" set "LT_HB_ARG=-DFETCHCONTENT_SOURCE_DIR_HARFBUZZ=%~dp0build-min-nmake\_deps\harfbuzz-src"
if exist "%~dp0build-min-nmake\_deps\freetype-src\CMakeLists.txt" set "LT_FT_ARG=-DFETCHCONTENT_SOURCE_DIR_FREETYPE=%~dp0build-min-nmake\_deps\freetype-src"
cmake -S "%~dp0." -B "%~dp0out\build-sdk" -G "Ninja Multi-Config" -DLUMATEXT_BUILD_SHARED=ON -DLUMATEXT_BUILD_STATIC=ON -DLUMATEXT_BUILD_TESTS=ON -DLUMATEXT_BUILD_SAMPLES=OFF "%LT_HB_ARG%" "%LT_FT_ARG%" %*
if errorlevel 1 exit /b 1
for %%C in (Debug Release) do (
  cmake --build "%~dp0out\build-sdk" --config %%C --parallel 4
  if errorlevel 1 exit /b 1
  ctest --test-dir "%~dp0out\build-sdk" -C %%C --output-on-failure
  if errorlevel 1 exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package-sdk.ps1"
exit /b %errorlevel%
