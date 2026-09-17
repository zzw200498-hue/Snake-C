@echo off
chcp 65001 >nul
setlocal

rem ===========================================================================
rem  贪吃蛇 3.0  ——  一键编译脚本
rem
rem  用法：双击本文件，或者在命令行执行 build.bat
rem
rem  它会自动找到 Visual Studio 的编译环境，然后编译成 build\Snake3.exe
rem  不需要改任何路径。如果找不到 VS，请看 README 里的"用 Dev-C++ / MinGW 编译"。
rem ===========================================================================

cd /d "%~dp0"

echo.
echo   ============================================
echo      贪吃蛇 3.0   正在编译...
echo   ============================================
echo.

rem ---- 找 vcvars64.bat ----
set "VCVARS="
for %%P in (
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do (
  if exist %%P set "VCVARS=%%~P"
)

if not defined VCVARS (
  rem 用 vswhere 再找一次（任何版本、任何安装位置）
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
      if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)

if not defined VCVARS (
  echo   [X] 没找到 Visual Studio 的 C 编译器。
  echo.
  echo   你还有两个办法：
  echo     1^) 装免费的 "Visual Studio Community"，安装时勾选
  echo        "使用 C++ 的桌面开发"  （推荐，装完直接双击本文件即可）
  echo     2^) 装 Dev-C++ 或 MinGW，然后看 README 里的命令行编译方法
  echo.
  pause
  exit /b 1
)

echo   编译器环境：%VCVARS%
call "%VCVARS%" >nul 2>nul

if not exist build mkdir build

rem ---- 编译 ----
rem  /utf-8    源文件是 UTF-8，必须告诉编译器
rem  /W3       常用警告级别
rem  /O2       优化
rem  /MT       静态链接 C 运行库，生成的 exe 不依赖 VC 运行库，方便发给别人
set "SRC=src\engine.c src\art.c src\game.c"

cl /nologo /utf-8 /W3 /O2 /MT /DNDEBUG ^
   /Fe:build\Snake3.exe /Fo:build\ ^
   %SRC% ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib

if errorlevel 1 (
  echo.
  echo   [X] 编译失败，请把上面的错误信息发给作者。
  echo.
  pause
  exit /b 1
)

rem 清理中间文件
del /q build\*.obj 2>nul

echo.
echo   ============================================
echo      编译成功！
echo.
echo      程序位置： build\Snake3.exe
echo      双击就能玩，也可以直接发给朋友。
echo   ============================================
echo.

choice /c YN /n /m "  现在打开游戏吗？(Y/N) "
if errorlevel 2 goto :end
start "" "build\Snake3.exe"

:end
endlocal