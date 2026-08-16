@echo off
rem HLSL Blend DXBC Translator (C++) - 批处理入口
rem 用法: run_translator.bat [输入文件] [输出文件]
chcp 65001 >nul
cd /d "%~dp0"
set EXE=x64\Release\hlsl_blend_dxbc_translator.exe
set DATA=data

if not exist "%EXE%" (
    echo [错误] 未找到 %EXE% ,请先构建 Release x64。
    goto :eof
)

if "%~1"=="" goto usage

echo 翻译: %~1
"%EXE%" -input "%~1" -output "%~2" -data "%DATA%"
goto :eof

:usage
echo 用法: run_translator.bat ^<输入HLSLBlend文件^> ^<输出文件^>
echo.
echo 示例:
echo   run_translator.bat "tests\sample.txt" out\sample_out.txt
echo.
echo 或者直接跑回归测试:
echo   bash tests\run_tests.sh
goto :eof
