---
name: hlsl-dxbc-translator
description: HLSL/DXBC 混合翻译器(3Dmigoto ShaderFixes 用)的开发与改进指南。输入是 DXBC sm5 汇编 + HLSL 混合标记(HLSLMov/DXBCMov/HLSL/HLSLSnippet/HLSLInit/HLSLTexture/HLSLSampler/HLSLFunctionImport),输出纯 DXBC sm5 汇编。包含:架构、构建测试命令、关键 bug 经验(DXBC 浮点比较返回 NaN、saturate 折叠只折叠最外层、sincos 参数映射)、验证方法论(fxc 交叉验证、被读分量对比、结构验证)。处理本项目时加载。
---

# HLSL/DXBC 混合翻译器 开发指南

用 C++17 写的翻译器,把"DXBC 汇编 + HLSL 混合标记"翻译回纯 DXBC sm5 汇编,供 3Dmigoto ShaderFixes 使用。输入语法和用户工作流见项目 `PROJECT_SUMMARY.md`。

## 常用命令

```bash
# 构建(MSBuild Release x64)
"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" hlsl_blend_dxbc_translator.sln /p:Configuration=Release /p:Platform=x64

# 回归(14 项)
bash tests/run_tests.sh

# 运行
x64\Release\hlsl_blend_dxbc_translator.exe -input <文件> -output <输出> -data data
# -data data = 函数库目录; -no-sat-fold = 禁用 saturate 折叠(排查 _sat 兼容性)
```

## 架构速览(src/)

- `types` HLSL 类型 / `swizzle` 4分量swizzle编码 / `ast` 节点 / `lexer`+`parser` 递归下降
- `symbols` 符号表(作用域栈)+ 寄存器分配(free list)
- `intrinsics` 集中式 intrinsic 表(~65 函数)
- `codegen` AST→DXBC(类型推断、saturate 折叠)
- `translator` 主循环(8 标记)+ 控制流 + 函数展开 / `function_lib` lib.txt 解析
- `main` CLI

## 关键 bug 经验(改翻译逻辑时务必遵守)

1. **DXBC 浮点比较(lt/ge/eq/ne)返回 0 或全1位(0xFFFFFFFF=NaN as float),不是 0.0/1.0**。必须加 `and dst, dst, l(0x3f800000)` 归一化(fxc 标准)。整数比较(ilt/ige)返回真 0/1,不需要。这是主光/阴影异常的历史根因。
2. **saturate 折叠只折叠最外层**:`saturate(a*b*c)` 生成 `mul;mul_sat`(单层)。在 `CodeGen::eval` 复杂表达式分支临时关闭 `fold_sat_`,否则内层也加 _sat 导致嵌套 saturate(结果错误)。
3. **sincos**:HLSL `sincos(angle, out_sin, out_cos)` → `sincos <sin>, <angle>, <cos>`(sin→第2参数)。
4. **函数局部变量遮蔽**:`translate_decl` 用 `lookup_current_scope`,否则局部变量会别名外部同名变量。
5. **swizzle 填充**:复制第一个有效分量(`xy→xyxx`, `xyz→xyzx`, `zw→zwzz`),匹配 fxc/3Dmigoto 惯例。
6. **寄存器分配**:临时用后即还(free list);注意变量/临时生命周期不要重叠。
7. **HLSLInit**:清零所有分配过的寄存器,避免脏数据。

## 验证方法论(改完必须验证)

1. **fxc 交叉验证**(最重要):把 HLSL 片段(含自定义函数)提取成独立 shader,`fxc -T ps_5_0 -E main -Od -Fc out.asm in.hlsl`,对比 fxc 和我的翻译的指令序列/语义。fxc 路径:`C:\Program Files (x86)\Windows Kits\10\bin\10.0.17763.0\x64\fxc.exe`
2. **被读分量对比**:对比 mod(正常版)和我的输出,源 swizzle 前 N 位(被读分量)应一致(填充位无关)。
3. **结构验证**:`python tests/validate_asm.py <输出>`
4. **回归**:`bash tests/run_tests.sh`
5. **人工排查**(用户会配合):改最终输出行显示中间值(如 `mov o0.xyz, rX.xxx`),逐个看哪个值不对。

## 参考

- DXC(主,读源码):`E:\Project\Open\DirectXShaderCompiler`(HLSL 前端 tools/clang/lib/,token 规范 include/dxc/Support/d3d12TokenizedProgramFormat.hpp;DXC 不生成 DXBC 汇编)
- fxc(黑盒验证,见上)
- vkd3d(辅助,swizzle 函数移植来源):`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`

## 文档

- `PROJECT_SUMMARY.md` 速查手册 / `PROJECT_NOTES.md` 详细进度和 bug 记录 / `tests/` 测试样例
