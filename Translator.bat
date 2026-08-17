@echo off
chcp 65001 >nul
x64\Release\hlsl_blend_dxbc_translator.exe -output "E:\SteamLibrary\steamapps\common\Devil May Cry 5\ShaderFixes\695c8c0feada9292-ps.txt" -data data -input "tests\M2全局光照695c8c0feada9292-ps-HLSLBlend_v0.5.txt"
x64\Release\hlsl_blend_dxbc_translator.exe -output "E:\SteamLibrary\steamapps\common\Devil May Cry 5\ShaderFixes\70b537c6e90cf154-ps.txt" -data data -input "tests\星见雅mod身体部分材质70b537c6e90cf154-ps-HLSLBlend.txt"
x64\Release\hlsl_blend_dxbc_translator.exe -output "E:\SteamLibrary\steamapps\common\Devil May Cry 5\ShaderFixes\47af6e5c0fda4527-ps.txt" -data data -input "tests\星见雅modpbr材质湿身状态shader47af6e5c0fda4527-ps-HLSLBlend.txt"