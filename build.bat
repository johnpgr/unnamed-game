@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1" set release=0
if "%release%"=="1" set debug=0

set ROOT_DIR=%cd%
set BIN_DIR=%ROOT_DIR%\bin
set APP_MAIN=%ROOT_DIR%\src\app\editor_main.cpp
set RGFW_IMPL_CPP=%ROOT_DIR%\src\third_party\rgfw\rgfw_impl.cpp
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%BIN_DIR%\shaders" mkdir "%BIN_DIR%\shaders"

set COMMON_FLAGS=/std:c++14 /nologo /W4 /WX /wd4505 /wd4127 /I"%ROOT_DIR%\src" /DASSET_DIR=\"%BIN_DIR%\"
if "%debug%"=="1" set MODE_FLAGS=/Od /Zi
if "%release%"=="1" set MODE_FLAGS=/O2 /DNDEBUG

where glslangValidator >nul 2>nul
if "%ERRORLEVEL%"=="0" (
  glslangValidator -V "%ROOT_DIR%\assets\shaders\sprite.vert" -o "%BIN_DIR%\shaders\sprite.vert.spv" || exit /b 1
  glslangValidator -V "%ROOT_DIR%\assets\shaders\sprite.frag" -o "%BIN_DIR%\shaders\sprite.frag.spv" || exit /b 1
) else (
  where glslc >nul 2>nul || (echo missing shader compiler: glslangValidator or glslc & exit /b 1)
  glslc "%ROOT_DIR%\assets\shaders\sprite.vert" -o "%BIN_DIR%\shaders\sprite.vert.spv" || exit /b 1
  glslc "%ROOT_DIR%\assets\shaders\sprite.frag" -o "%BIN_DIR%\shaders\sprite.frag.spv" || exit /b 1
)

for %%f in (
  "%ROOT_DIR%\src\base\base_arena.cpp"
  "%ROOT_DIR%\src\base\base_log.cpp"
  "%ROOT_DIR%\src\base\base_mod.cpp"
  "%ROOT_DIR%\src\base\base_string.cpp"
  "%ROOT_DIR%\src\os\os_memory_win32.cpp"
  "%ROOT_DIR%\src\os\os_mod.cpp"
  "%ROOT_DIR%\src\os\os_threads_win32.cpp"
  "%ROOT_DIR%\src\draw\draw_core.cpp"
  "%ROOT_DIR%\src\draw\draw_mod.cpp"
  "%ROOT_DIR%\src\editor\editor_core.cpp"
  "%ROOT_DIR%\src\editor\editor_input.cpp"
  "%ROOT_DIR%\src\editor\editor_mod.cpp"
  "%ROOT_DIR%\src\render\render_mod.cpp"
  "%ROOT_DIR%\src\render\vulkan.cpp"
  "%ROOT_DIR%\src\text\text_buffer.cpp"
  "%ROOT_DIR%\src\text\text_mod.cpp"
  "%ROOT_DIR%\src\third_party\rgfw\rgfw_impl.cpp"
) do (
  cl %COMMON_FLAGS% %MODE_FLAGS% /Zs /TP "%%~f" || exit /b 1
)

for %%f in (
  "%ROOT_DIR%\src\base\base_arena.h"
  "%ROOT_DIR%\src\base\base_mod.h"
  "%ROOT_DIR%\src\base\base_core.h"
  "%ROOT_DIR%\src\base\base_list.h"
  "%ROOT_DIR%\src\base\base_log.h"
  "%ROOT_DIR%\src\base\base_string.h"
  "%ROOT_DIR%\src\base\base_threads.h"
  "%ROOT_DIR%\src\base\base_types.h"
  "%ROOT_DIR%\src\draw\draw_core.h"
  "%ROOT_DIR%\src\draw\draw_mod.h"
  "%ROOT_DIR%\src\editor\editor_core.h"
  "%ROOT_DIR%\src\editor\editor_input.h"
  "%ROOT_DIR%\src\editor\editor_mod.h"
  "%ROOT_DIR%\src\render\render_mod.h"
  "%ROOT_DIR%\src\render\vulkan.h"
  "%ROOT_DIR%\src\text\text_buffer.h"
  "%ROOT_DIR%\src\text\text_mod.h"
  "%ROOT_DIR%\src\os\os_mod.h"
  "%ROOT_DIR%\src\third_party\rgfw\RGFW.h"
) do (
  cl %COMMON_FLAGS% %MODE_FLAGS% /Zs /TP "%%~f" || exit /b 1
)

cl %COMMON_FLAGS% %MODE_FLAGS% /EHsc "%APP_MAIN%" "%RGFW_IMPL_CPP%" /link vulkan-1.lib /out:"%BIN_DIR%\main.exe" || exit /b 1
echo built %BIN_DIR%\main.exe
