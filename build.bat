@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0.") do set "ROOT_DIR=%%~fI"
set "FLAGS_FILE=%ROOT_DIR%\compile_flags.txt"
set "BUILD_DIR=%ROOT_DIR%\build"
set "MAIN_SOURCE_FILE=%ROOT_DIR%\src\main.cpp"
set "ERROR_SOURCE_FILE=%ROOT_DIR%\src\platform\error.cpp"
set "RENDERER_SOURCE_FILE=%ROOT_DIR%\src\renderer\vulkan.cpp"
set "GAME_SOURCE_FILE=%ROOT_DIR%\src\game.cpp"
set "SDL_SOURCE_DIR=%ROOT_DIR%\lib\SDL"
set "GLM_SOURCE_DIR=%ROOT_DIR%\lib\glm"

if defined BUILD_MODE (
  set "MODE=%BUILD_MODE%"
) else (
  set "MODE=debug"
)

if /I "%~1"=="debug" (
  set "MODE=debug"
  shift
) else if /I "%~1"=="release" (
  set "MODE=release"
  shift
)

if "%~1"=="" (
  set "OUTPUT_FILE=%BUILD_DIR%\main.exe"
) else (
  set "OUTPUT_FILE=%~1"
)

if defined CXX (
  set "COMPILER=%CXX%"
) else (
  set "COMPILER=clang++"
)

if defined CC (
  set "C_COMPILER=%CC%"
) else (
  set "C_COMPILER=clang"
)

if defined VULKAN_SDK (
  set "VULKAN_SDK_DIR=%VULKAN_SDK%"
) else (
  set "VULKAN_SDK_DIR=C:\VulkanSDK\1.4.341.1"
)

set "GAME_OUTPUT_FILE=%BUILD_DIR%\game.dll"
set "GAME_LINK_FLAGS=-shared"
set "WINDOWS_RUNTIME_FLAGS=-fms-runtime-lib=dll"
set "WINDOWS_SDL_SYSTEM_LIBS=-lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32"
set "VULKAN_LINK_FLAGS=-L%VULKAN_SDK_DIR%\Lib -lvulkan-1"
set "SDL_BUILD_DIR=%BUILD_DIR%\sdl-%MODE%"
set "SDL_INSTALL_DIR=%SDL_BUILD_DIR%\install"
set "CMAKE_BUILD_TYPE=Debug"
if /I "%MODE%"=="release" set "CMAKE_BUILD_TYPE=Release"

if not exist "%FLAGS_FILE%" (
  >&2 echo Missing compile flags file: %FLAGS_FILE%
  exit /b 1
)

for %%F in ("%MAIN_SOURCE_FILE%" "%ERROR_SOURCE_FILE%" "%RENDERER_SOURCE_FILE%" "%GAME_SOURCE_FILE%") do (
  if not exist "%%~F" (
    >&2 echo Missing source file: %%~F
    exit /b 1
  )
)

if not exist "%SDL_SOURCE_DIR%\CMakeLists.txt" (
  >&2 echo Missing SDL submodule under lib\. Run: git submodule update --init --recursive
  exit /b 1
)

if not exist "%GLM_SOURCE_DIR%\glm\glm.hpp" (
  >&2 echo Missing GLM submodule under lib\. Run: git submodule update --init --recursive
  exit /b 1
)

if not exist "%VULKAN_SDK_DIR%\Include\vulkan\vulkan.h" (
  >&2 echo Missing Vulkan headers: %VULKAN_SDK_DIR%\Include\vulkan\vulkan.h
  exit /b 1
)

if not exist "%VULKAN_SDK_DIR%\Lib\vulkan-1.lib" (
  >&2 echo Missing Vulkan import library: %VULKAN_SDK_DIR%\Lib\vulkan-1.lib
  exit /b 1
)

where /q "%COMPILER%"
if errorlevel 1 (
  >&2 echo Missing required compiler: %COMPILER%
  exit /b 1
)

where /q "%C_COMPILER%"
if errorlevel 1 (
  >&2 echo Missing required C compiler: %C_COMPILER%
  exit /b 1
)

where /q cmake
if errorlevel 1 (
  >&2 echo Missing required command: cmake
  exit /b 1
)

where /q ninja
if errorlevel 1 (
  >&2 echo Missing required command: ninja
  exit /b 1
)

if /I "%MODE%"=="release" (
  set "MODE_FLAGS=-O3 -DNDEBUG"
) else (
  set "MODE_FLAGS=-Og -g3 -DDEBUG"
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "FLAGS="
for /f "usebackq tokens=* delims=" %%L in ("%FLAGS_FILE%") do (
  set "LINE=%%L"
  for /f "tokens=* delims= " %%A in ("!LINE!") do set "LINE=%%A"
  if defined LINE (
    if /I "%MODE%"=="release" (
      if /I not "!LINE:~0,11!"=="-fsanitize" set "FLAGS=!FLAGS! !LINE!"
    ) else (
      set "FLAGS=!FLAGS! !LINE!"
    )
  )
)

call cmake -S "%SDL_SOURCE_DIR%" -B "%SDL_BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% -DCMAKE_INSTALL_PREFIX="%SDL_INSTALL_DIR%" -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=%C_COMPILER% -DCMAKE_CXX_COMPILER=%COMPILER% -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_STATIC_PIC=ON -DSDL_INSTALL=ON -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF
if errorlevel 1 exit /b %errorlevel%

call cmake --build "%SDL_BUILD_DIR%" --config %CMAKE_BUILD_TYPE% --target SDL3-static
if errorlevel 1 exit /b %errorlevel%

call cmake --install "%SDL_BUILD_DIR%" --config %CMAKE_BUILD_TYPE%
if errorlevel 1 exit /b %errorlevel%

set "SDL_LIB_FILE="
for /f "delims=" %%I in ('dir /b /s "%SDL_INSTALL_DIR%\SDL3-static.lib" 2^>nul') do (
  set "SDL_LIB_FILE=%%I"
  goto :have_sdl_lib
)
for /f "delims=" %%I in ('dir /b /s "%SDL_INSTALL_DIR%\SDL3.lib" 2^>nul') do (
  set "SDL_LIB_FILE=%%I"
  goto :have_sdl_lib
)

>&2 echo Failed to locate the installed SDL static library.
exit /b 1

:have_sdl_lib
call "%COMPILER%" %CPPFLAGS% %CFLAGS% %CXXFLAGS% %WINDOWS_RUNTIME_FLAGS% !FLAGS! %MODE_FLAGS% -I"%ROOT_DIR%\src" -I"%GLM_SOURCE_DIR%" -I"%SDL_INSTALL_DIR%\include" -I"%VULKAN_SDK_DIR%\Include" "%MAIN_SOURCE_FILE%" "%ERROR_SOURCE_FILE%" "%RENDERER_SOURCE_FILE%" %LDFLAGS% "%SDL_LIB_FILE%" %WINDOWS_SDL_SYSTEM_LIBS% %VULKAN_LINK_FLAGS% -o "%OUTPUT_FILE%"
if errorlevel 1 exit /b %errorlevel%

call "%COMPILER%" %CPPFLAGS% %CFLAGS% %CXXFLAGS% %WINDOWS_RUNTIME_FLAGS% !FLAGS! %MODE_FLAGS% -I"%ROOT_DIR%\src" -I"%GLM_SOURCE_DIR%" "%GAME_SOURCE_FILE%" %LDFLAGS% %GAME_LINK_FLAGS% -o "%GAME_OUTPUT_FILE%"
if errorlevel 1 exit /b %errorlevel%

echo Built (%MODE%): %OUTPUT_FILE%
echo Built (%MODE%): %GAME_OUTPUT_FILE%
exit /b 0
