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
cd src
rc /nologo /c65001 /fo ..\build\app.res app.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W3 /LD panel.cpp /Fo:..\build\panel.obj /Fe:..\build\QuickAudio.Hook.0.2.0.dll /link /DEF:panel.def /IMPLIB:..\build\QuickAudio.Hook.lib runtimeobject.lib ole32.lib oleaut32.lib user32.lib advapi32.lib shell32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W3 backend.cpp ..\build\app.res /Fo:..\build\backend.obj /Fe:..\build\QuickAudio.exe /link /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup runtimeobject.lib ole32.lib oleaut32.lib user32.lib advapi32.lib shell32.lib uuid.lib comctl32.lib gdi32.lib winhttp.lib bcrypt.lib
exit /b %errorlevel%
