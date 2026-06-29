@echo off
REM ============================================================================
REM  instalar_vst3.bat — copia o .vst3 compilado para a pasta VST3 do SISTEMA.
REM  PRECISA SER RODADO COMO ADMINISTRADOR (a pasta Program Files e protegida).
REM
REM  Alternativa SEM admin: no Ableton, Preferences -> Plug-Ins -> ative
REM  "Use VST3 Plug-In Custom Folder" e aponte para a pasta:
REM     plugin\build\TccAutotune_artefacts\Release\VST3
REM ============================================================================
set SRC=%~dp0build\TccAutotune_artefacts\Release\VST3\TCC Autotune.vst3
set DST=%CommonProgramFiles%\VST3\TCC Autotune.vst3
if not exist "%SRC%" (
  echo ERRO: nao achei o .vst3 compilado em "%SRC%".
  echo Compile primeiro com build.bat.
  goto fim
)
echo Copiando "%SRC%"
echo      -^> "%DST%"
robocopy "%SRC%" "%DST%" /MIR /NJH /NJS /NDL /NP
if %errorlevel% GEQ 8 ( echo FALHOU ^(rodou como admin?^) ) else ( echo OK: instalado no sistema. Re-escaneie os plugins no Ableton. )
:fim
