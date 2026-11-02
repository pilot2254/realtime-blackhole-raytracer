@echo off
REM Build Gargantua Studio.
REM MSVC: run from "x64 Native Tools Command Prompt for VS".  MinGW: run from a shell with g++ on PATH.
set IMGUI=imgui
set APP=src\main.cpp src\app\app.cpp ^
    src\core\gl.cpp src\core\util.cpp ^
    src\sim\camera.cpp src\sim\physics.cpp src\sim\scene.cpp ^
    src\film\sequence.cpp src\film\gargantua_film.cpp ^
    src\render\shaders_scene.cpp src\render\shaders_post.cpp src\render\renderer.cpp src\render\overlay.cpp src\render\export.cpp src\render\refine.cpp ^
    src\audio\synth.cpp src\audio\audio.cpp ^
    src\editor\theme.cpp src\editor\panels.cpp src\editor\sequencer.cpp src\editor\editor.cpp
set LIB=%IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp ^
    %IMGUI%\backends\imgui_impl_win32.cpp %IMGUI%\backends\imgui_impl_opengl3.cpp

where cl >nul 2>nul
if %errorlevel%==0 (
    if not exist build mkdir build
    cl /nologo /utf-8 /O2 /EHsc /std:c++17 /Isrc /I%IMGUI% /I%IMGUI%\backends %APP% %LIB% /Fobuild\ /Fe:gargantua_gl.exe
) else (
    g++ -O2 -std=c++17 -Isrc -I%IMGUI% -I%IMGUI%\backends %APP% %LIB% -o gargantua_gl.exe -static ^
        -lopengl32 -lgdi32 -luser32 -ldwmapi -lcomdlg32 -lshell32 -lwinmm -mwindows
)
