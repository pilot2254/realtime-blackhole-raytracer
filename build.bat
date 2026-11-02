@echo off
REM Build Gargantua Studio.
REM MSVC: run from "x64 Native Tools Command Prompt for VS".  MinGW: run from a shell with g++ on PATH.
set IMGUI=imgui
set SRC=main.cpp %IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp %IMGUI%\backends\imgui_impl_win32.cpp %IMGUI%\backends\imgui_impl_opengl3.cpp

where cl >nul 2>nul
if %errorlevel%==0 (
    cl /nologo /O2 /EHsc /std:c++17 /I%IMGUI% /I%IMGUI%\backends %SRC% /Fe:gargantua_studio.exe
) else (
    g++ -O2 -std=c++17 -I%IMGUI% -I%IMGUI%\backends %SRC% -o gargantua_gl.exe -static -lopengl32 -lgdi32 -luser32 -ldwmapi -mwindows
)
