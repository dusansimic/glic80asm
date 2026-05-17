@echo off
rem Pipeline: sdcc -> preprocess (prepend startup stub) -> glic80asm -ec
rem
rem Usage:
rem   tools\compile.bat program.c [-o program.bin] [--stack 0x7700]
rem
rem Env overrides:
rem   SDCC=...        path to sdcc binary (default: sdcc-sdcc.exe or sdcc.exe on PATH)
rem   GLIC80ASM=...   path to glic80asm.exe (default: glic80asm.exe next to this script's repo root)

setlocal EnableDelayedExpansion

if "%~1"=="" goto :usage

set "SRC="
set "OUT="
set "STACK=0x7700"

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="-o" (
    set "OUT=%~2"
    shift
    shift
    goto :parse_args
)
if /I "%~1"=="--stack" (
    set "STACK=%~2"
    shift
    shift
    goto :parse_args
)
if /I "%~1"=="-h" goto :usage
if /I "%~1"=="--help" goto :usage
if "%~1:~0,1%"=="-" (
    echo unknown option: %~1
    exit /b 2
)
if not "!SRC!"=="" (
    echo multiple inputs not supported
    exit /b 2
)
set "SRC=%~1"
shift
goto :parse_args

:args_done
if "!SRC!"=="" goto :usage
if "!OUT!"=="" set "OUT=%~dpn1.bin"

if not defined SDCC (
    where sdcc-sdcc.exe >nul 2>nul && set "SDCC=sdcc-sdcc"
    if not defined SDCC (
        where sdcc.exe >nul 2>nul && set "SDCC=sdcc"
    )
)
if not defined SDCC (
    echo sdcc not found; install sdcc or set SDCC=path\to\sdcc.exe
    exit /b 2
)

if not defined GLIC80ASM (
    set "GLIC80ASM=%~dp0..\glic80asm.exe"
)
if not exist "!GLIC80ASM!" (
    where glic80asm.exe >nul 2>nul && set "GLIC80ASM=glic80asm.exe"
)
if not exist "!GLIC80ASM!" (
    echo glic80asm not found; build it (make) or set GLIC80ASM=path\to\glic80asm.exe
    exit /b 2
)

set "TMP_ASM=%TEMP%\glic80_%RANDOM%.asm"
set "TMP_FULL=%TEMP%\glic80_%RANDOM%_full.asm"

"!SDCC!" -mz80 -S -o "!TMP_ASM!" "!SRC!" || (
    if exist "!TMP_ASM!" del "!TMP_ASM!"
    exit /b 1
)

> "!TMP_FULL!" (
    echo     org $0000
    echo     ld  sp, !STACK!
    echo     call _main
    echo __glic80_halt:
    echo     jr __glic80_halt
)
type "!TMP_ASM!" >> "!TMP_FULL!"

"!GLIC80ASM!" -ec -o "!OUT!" "!TMP_FULL!"
set "RC=%errorlevel%"

del "!TMP_ASM!" "!TMP_FULL!" 2>nul
exit /b !RC!

:usage
echo usage: %~nx0 program.c [-o program.bin] [--stack 0x7700]
exit /b 2
