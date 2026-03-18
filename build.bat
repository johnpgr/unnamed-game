@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0.") do set "ROOT_DIR=%%~fI"
set "FLAGS_FILE=%ROOT_DIR%\compile_flags.txt"
set "BUILD_DIR=%ROOT_DIR%\build"
set "MAIN_SOURCE_FILE=%ROOT_DIR%\src\main.cpp"
set "GAME_SOURCE_FILE=%ROOT_DIR%\src\game.cpp"
set "SDL_SOURCE_DIR=%ROOT_DIR%\lib\SDL"
set "GLM_SOURCE_DIR=%ROOT_DIR%\lib\glm"

if defined BUILD_MODE (
  set "MODE=%BUILD_MODE%"
) else (
  set "MODE=debug"
)

set "BUILD_SDL=0"
set "OUTPUT_FILE="

:parse_args
if "%~1"=="" goto :args_done

if /I "%~1"=="debug" (
  set "MODE=debug"
  shift
  goto :parse_args
)

if /I "%~1"=="release" (
  set "MODE=release"
  shift
  goto :parse_args
)

if /I "%~1"=="sdl" (
  set "BUILD_SDL=1"
  shift
  goto :parse_args
)

if /I "%~1"=="--sdl" (
  set "BUILD_SDL=1"
  shift
  goto :parse_args
)

if not defined OUTPUT_FILE (
  set "OUTPUT_FILE=%~1"
  shift
  goto :parse_args
)

>&2 echo Unexpected argument: %~1
>&2 echo Usage: %~nx0 [debug^|release] [sdl^|--sdl] [output_path]
exit /b 1

:args_done
if not defined OUTPUT_FILE set "OUTPUT_FILE=%BUILD_DIR%\main.exe"

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

set "GAME_OUTPUT_FILE=%BUILD_DIR%\game.dll"
set "GAME_LINK_FLAGS=-shared"
set "WINDOWS_RUNTIME_FLAGS=-fms-runtime-lib=dll"
set "WINDOWS_SDL_SYSTEM_LIBS=-lkernel32 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -ladvapi32 -lsetupapi -lshell32"
set "SDL_BUILD_DIR=%BUILD_DIR%\sdl-windows-%MODE%"
set "SDL_INSTALL_DIR=%SDL_BUILD_DIR%\install"
set "CMAKE_BUILD_TYPE=Debug"
if /I "%MODE%"=="release" set "CMAKE_BUILD_TYPE=Release"

if defined VULKAN_SDK (
  set "VULKAN_SDK_DIR=%VULKAN_SDK%"
) else (
  set "VULKAN_SDK_DIR="
  for /f "delims=" %%I in ('dir /b /ad /o-n "C:\VulkanSDK" 2^>nul') do (
    if not defined VULKAN_SDK_DIR set "VULKAN_SDK_DIR=C:\VulkanSDK\%%I"
  )
)

if not exist "%FLAGS_FILE%" (
  >&2 echo Missing compile flags file: %FLAGS_FILE%
  exit /b 1
)

for %%F in ("%MAIN_SOURCE_FILE%" "%GAME_SOURCE_FILE%") do (
  if not exist "%%~F" (
    >&2 echo Missing source file: %%~F
    exit /b 1
  )
)

if not exist "%GLM_SOURCE_DIR%\glm\glm.hpp" (
  >&2 echo Missing GLM dependency under lib\. Run: git submodule update --init --recursive
  exit /b 1
)

if "%BUILD_SDL%"=="1" (
  if not exist "%SDL_SOURCE_DIR%\CMakeLists.txt" (
    >&2 echo Missing SDL dependency under lib\. Run: git submodule update --init --recursive
    exit /b 1
  )
)

if not defined VULKAN_SDK_DIR (
  >&2 echo Missing Vulkan SDK. Set VULKAN_SDK or install it under C:\VulkanSDK.
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

call :require_command "%COMPILER%" "compiler"
if errorlevel 1 exit /b %errorlevel%

call :require_command "%C_COMPILER%" "C compiler"
if errorlevel 1 exit /b %errorlevel%

if "%BUILD_SDL%"=="1" (
  call :require_command cmake "cmake"
  if errorlevel 1 exit /b %errorlevel%
  call :require_command ninja "ninja"
  if errorlevel 1 exit /b %errorlevel%
)

set "VULKAN_INCLUDE_FLAGS=-I%VULKAN_SDK_DIR%\Include"
set "VULKAN_LINK_FLAGS=-L%VULKAN_SDK_DIR%\Lib -lvulkan-1"
set "MAIN_LINK_FLAGS=%VULKAN_LINK_FLAGS%"

set "MODE_FLAGS=-Og -g3 -DDEBUG"
if /I "%MODE%"=="release" set "MODE_FLAGS=-O3 -DNDEBUG"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "FLAGS="
set "SKIP_NEXT=0"
for /f "usebackq tokens=* delims=" %%L in ("%FLAGS_FILE%") do (
  set "LINE=%%L"
  for /f "tokens=* delims= " %%A in ("!LINE!") do set "LINE=%%A"
  if "!SKIP_NEXT!"=="1" (
    set "SKIP_NEXT=0"
  ) else if defined LINE (
    if /I "!LINE!"=="-isysroot" (
      set "SKIP_NEXT=1"
    ) else if /I "!LINE:~0,2!"=="-I" if "!LINE:~2,1!"=="/" (
      rem Skip Unix-only include paths on Windows.
    ) else if /I "%MODE%"=="release" if /I "!LINE:~0,11!"=="-fsanitize" (
      rem Skip sanitizers in release builds.
    ) else (
      set "FLAGS=!FLAGS! !LINE!"
    )
  )
)

if "%BUILD_SDL%"=="1" (
  call cmake -S "%SDL_SOURCE_DIR%" -B "%SDL_BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% -DCMAKE_INSTALL_PREFIX="%SDL_INSTALL_DIR%" -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=%C_COMPILER% -DCMAKE_CXX_COMPILER=%COMPILER% -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_STATIC_PIC=ON -DSDL_INSTALL=ON -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF
  if errorlevel 1 exit /b %errorlevel%

  call cmake --build "%SDL_BUILD_DIR%" --config %CMAKE_BUILD_TYPE% --target SDL3-static
  if errorlevel 1 exit /b %errorlevel%

  call cmake --install "%SDL_BUILD_DIR%" --config %CMAKE_BUILD_TYPE%
  if errorlevel 1 exit /b %errorlevel%
)

set "SDL_LIB_FILE="
for /f "delims=" %%I in ('dir /b /s "%SDL_INSTALL_DIR%\SDL3-static.lib" 2^>nul') do (
  set "SDL_LIB_FILE=%%I"
  goto :have_sdl_lib
)
for /f "delims=" %%I in ('dir /b /s "%SDL_INSTALL_DIR%\SDL3.lib" 2^>nul') do (
  set "SDL_LIB_FILE=%%I"
  goto :have_sdl_lib
)

>&2 echo Missing built SDL3 artifacts in %SDL_INSTALL_DIR%. Run: %~nx0 %MODE% sdl
exit /b 1

:have_sdl_lib
call "%COMPILER%" %CPPFLAGS% %CFLAGS% %CXXFLAGS% %WINDOWS_RUNTIME_FLAGS% !FLAGS! %MODE_FLAGS% %VULKAN_INCLUDE_FLAGS% -I"%ROOT_DIR%\src" -I"%GLM_SOURCE_DIR%" -I"%SDL_INSTALL_DIR%\include" "%MAIN_SOURCE_FILE%" %LDFLAGS% "%SDL_LIB_FILE%" %MAIN_LINK_FLAGS% %WINDOWS_SDL_SYSTEM_LIBS% -o "%OUTPUT_FILE%"
if errorlevel 1 exit /b %errorlevel%

call "%COMPILER%" %CPPFLAGS% %CFLAGS% %CXXFLAGS% %WINDOWS_RUNTIME_FLAGS% !FLAGS! %MODE_FLAGS% -I"%ROOT_DIR%\src" -I"%GLM_SOURCE_DIR%" "%GAME_SOURCE_FILE%" %LDFLAGS% %GAME_LINK_FLAGS% -o "%GAME_OUTPUT_FILE%"
if errorlevel 1 exit /b %errorlevel%

echo Built (%MODE%^): %OUTPUT_FILE%
echo Built (%MODE%^): %GAME_OUTPUT_FILE%
exit /b 0

:require_command
where /q %~1
if errorlevel 1 (
  >&2 echo Missing required %~2: %~1
  exit /b 1
)
exit /b 0
