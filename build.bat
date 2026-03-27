@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Unpack Arguments --------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%~1"==""                      set editor=1
if "%~1"=="debug"   if "%~2"=="" set editor=1
if "%~1"=="release" if "%~2"=="" set editor=1

:: --- Paths -------------------------------------------------------------------
set root_dir=%cd%
set bin_dir=%root_dir%\bin
set src_dir=%root_dir%\src
set bin_dir_fwd=%bin_dir:\=/%
if not exist "%bin_dir%" mkdir "%bin_dir%"

:: --- Compile/Link Line Definitions -------------------------------------------
set vendor_dir=%root_dir%\vendor
set vulkan_dir=%vendor_dir%\vulkan
set glfw_dir=%vendor_dir%\glfw
set common=/std:c++14 /nologo /W4 /WX /wd4505 /wd4127 /wd4201 /wd4996 /I"%src_dir%" /I"%vulkan_dir%\Include" /I"%glfw_dir%\include" /DASSET_DIR=\"%bin_dir_fwd%\"
if "%debug%"=="1"   set compile=cl %common% /Od /Zi
if "%release%"=="1" set compile=cl %common% /O2 /DNDEBUG
set libs=/LIBPATH:"%vulkan_dir%\Lib" /LIBPATH:"%glfw_dir%\lib" vulkan-1.lib glfw3_mt.lib user32.lib gdi32.lib shell32.lib

:: --- Shaders -----------------------------------------------------------------
:: Compile shaders if forced or if .spv files are missing
if not exist "%bin_dir%\shaders" mkdir "%bin_dir%\shaders"
if not exist "%bin_dir%\shaders\sprite.vert.spv" set shaders=1
if not exist "%bin_dir%\shaders\sprite.frag.spv" set shaders=1
if "%shaders%"=="1" (
  where glslangValidator >nul 2>nul
  if "!ERRORLEVEL!"=="0" (
    glslangValidator -V "%root_dir%\assets\shaders\sprite.vert" -o "%bin_dir%\shaders\sprite.vert.spv" || exit /b 1
    glslangValidator -V "%root_dir%\assets\shaders\sprite.frag" -o "%bin_dir%\shaders\sprite.frag.spv" || exit /b 1
  ) else (
    where glslc >nul 2>nul || (echo missing shader compiler: glslangValidator or glslc & exit /b 1)
    glslc "%root_dir%\assets\shaders\sprite.vert" -o "%bin_dir%\shaders\sprite.vert.spv" || exit /b 1
    glslc "%root_dir%\assets\shaders\sprite.frag" -o "%bin_dir%\shaders\sprite.frag.spv" || exit /b 1
  )
  echo compiled shaders
)

:: --- Build Targets -----------------------------------------------------------
pushd "%bin_dir%"
if "%editor%"=="1" (
  set didbuild=1
  %compile% "%src_dir%\app\editor_main.cpp" /link %libs% /out:"%bin_dir%\main.exe" || exit /b 1
)
popd

:: --- Warn On No Builds -------------------------------------------------------
if not "%didbuild%"=="1" (
  echo [WARNING] no valid build target. usage: build [editor] [debug^|release] [shaders]
  exit /b 1
)
echo built %bin_dir%\main.exe
