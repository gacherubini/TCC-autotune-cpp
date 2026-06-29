@echo off
REM Compila o projeto. Rode este arquivo sempre que mudar o codigo.
REM Garante que o g++ do scoop esteja no PATH desta janela:
set PATH=%USERPROFILE%\scoop\apps\gcc\current\bin;%PATH%

REM autotune.exe = offline (referencia) | autotune_rt.exe = causal/streaming (tempo real)
REM stream_test.exe = driver headless do nucleo de streaming (AutotuneStream)
REM src/core         = dsp.h (compartilhado por offline/causal/C1/C2)
REM src/offline_causal = main.cpp (offline) + autotune_rt.cpp (causal)
REM src/c1_streaming   = autotune_stream.h (nucleo C1) + stream_test.cpp (driver)
g++ -std=c++17 -O2 -I external src/offline_causal/main.cpp        -o autotune.exe        || goto erro
g++ -std=c++17 -O2 -I external src/offline_causal/autotune_rt.cpp -o autotune_rt.exe     || goto erro
g++ -std=c++17 -O2 -I external src/c1_streaming/stream_test.cpp   -o stream_test.exe    || goto erro

echo.
echo COMPILOU OK -^> autotune.exe, autotune_rt.exe e stream_test.exe
goto fim

:erro
echo.
echo ERRO na compilacao

:fim
