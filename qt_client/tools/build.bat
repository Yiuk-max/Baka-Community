@echo off
rem ---------------------------------------------------------------------------
rem Fixed toolchain build (avoid msys64 g++/cmake on system PATH).
rem   tools\build.bat          configure + build
rem   tools\build.bat deploy   build, then copy Qt runtime with windeployqt
rem ---------------------------------------------------------------------------
setlocal
set ROOT=%~dp0..
set QT_PREFIX=D:/QT/6.11.1/mingw_64
set PATH=D:\QT\Tools\CMake_64\bin;D:\QT\Tools\Ninja;D:\QT\Tools\mingw1310_64\bin;%PATH%

cmake -S "%ROOT%" -B "%ROOT%\build" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=%QT_PREFIX% -DCMAKE_CXX_COMPILER=D:/QT/Tools/mingw1310_64/bin/g++.exe
if errorlevel 1 goto fail

cmake --build "%ROOT%\build" -j4
if errorlevel 1 goto fail

if /I "%1"=="deploy" (
  "%QT_PREFIX%/bin/windeployqt.exe" "%ROOT%\build\baka_client.exe"
  if errorlevel 1 goto fail
  echo deployed: %ROOT%\build
)

echo BUILD OK
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
