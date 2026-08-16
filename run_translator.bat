@echo off
rem HLSL Blend DXBC Translator (C++) - batch entry
rem Usage: run_translator.bat [input file] [output file]
cd /d "%~dp0"
set EXE=x64\Release\hlsl_blend_dxbc_translator.exe
set DATA=data

if not exist "%EXE%" (
    echo [ERROR] %EXE% not found. Build Release x64 first.
    goto :eof
)

if "%~1"=="" goto usage

echo Translating: %~1
"%EXE%" -input "%~1" -output "%~2" -data "%DATA%"
goto :eof

:usage
echo Usage: run_translator.bat ^<input HLSLBlend file^> ^<output file^>
echo.
echo Example:
echo   run_translator.bat "tests\sample.txt" out\sample_out.txt
echo.
echo Or run the regression suite:
echo   bash tests\run_tests.sh
goto :eof
