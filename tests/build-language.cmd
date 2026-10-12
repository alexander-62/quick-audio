@echo off
setlocal
cd /d "%~dp0.."
if not defined VSCMD_VER (
  for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "QA_VS=%%I"
)
if not defined VSCMD_VER if not defined QA_VS exit /b 1
if not defined VSCMD_VER call "%QA_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /utf-8 /MT tests\language.cpp /Fo:build\language-test.obj /Fe:build\language-test.exe /link advapi32.lib shell32.lib
exit /b %errorlevel%

