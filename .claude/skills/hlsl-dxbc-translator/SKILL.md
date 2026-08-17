---
name: hlsl-dxbc-translator
description: HLSL/DXBC 混合翻译器(3Dmigoto ShaderFixes 用)的开发、改进与调试指南。输入是 DXBC sm5 汇编 + HLSL 混合标记(HLSLMov/DXBCMov/HLSL/HLSLSnippet/HLSLInit/HLSLTexture/HLSLSampler/HLSLFunctionImport),输出纯 DXBC sm5 汇编。处理本项目(E:\Project\Cpp\hlsl_blend_dxbc_translator)或任何 HLSL/DXBC 翻译相关工作时必须加载,尤其是:修改翻译逻辑、修复 shader 翻译 bug、添加/调整 intrinsic、主光/阴影/光照翻译问题、swizzle/sincos/saturate/浮点比较处理、验证输出正确性。用户提到翻译器、DXBC 汇编、HLSL 混合语法、3Dmigoto shader、翻译质量改进、shader 渲染异常时都应加载。
---

# HLSL/DXBC 混合翻译器 开发指南

用 C++17 写的翻译器:把"DXBC 汇编 + HLSL 混合标记"翻译回纯 DXBC sm5 汇编,供 3Dmigoto ShaderFixes 使用。

## 快速入口

- **详细速查**(架构、命令、踩坑、验证、参考):先读项目根 `PROJECT_SUMMARY.md`
- **详细进度/bug 记录**:读 `PROJECT_NOTES.md`
- 本 SKILL.md 是浓缩版工作指南

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

1. **DXBC 浮点比较(lt/ge/eq/ne)返回 0 或全1位(0xFFFFFFFF=NaN as float),不是 0.0/1.0**。
   必须加 `and dst, dst, l(0x3f800000)` 归一化(fxc 标准做法,0x3f800000=float 1.0 位)。
   整数比较(ilt/ige/ieq/ult/uge)已返回真 0/1,不需要。
   **这是主光/阴影异常的历史根因** —— NaN 参与数学运算污染全链。
2. **saturate 折叠只折叠最外层**:`saturate(a*b*c)` 生成 `mul;mul_sat`(单层)。
   在 `CodeGen::eval` 复杂表达式分支临时关闭 `fold_sat_`,否则内层也加 _sat → 嵌套 saturate(结果错误)。
3. **sincos**:HLSL `sincos(angle, out_sin, out_cos)` → `sincos <sin>, <angle>, <cos>`(sin→第2参数)。
4. **函数局部变量遮蔽**:`translate_decl` 用 `lookup_current_scope`(只查当前作用域),否则局部变量别名外部同名变量。
5. **swizzle 填充**:复制第一个有效分量(`xy→xyxx`, `xyz→xyzx`, `zw→zwzz`),匹配 fxc/3Dmigoto 惯例。
6. **寄存器分配**:临时用后即还(free list);变量与临时生命周期不要重叠(复用冲突)。
7. **HLSLInit**:清零所有分配过的寄存器,避免脏数据。
8. **rgba swizzle 归一化**:HLSL 允许 `.rgb`/`.rgba`,parser 里非 xyzw 的成员名若全在 `rgba` 中要映射为 `xyzw`
   (`r→x g→y b→z a→w`),否则 `C.r` 会被当成 struct 成员走进 member access 报错。
9. **表达式取分量 `f(...).xyz`**:member access 处理——`eval(base)` 求值成 operand,再 `format_src(reg, swizzle, dm)` 发 mov;
   operand 为立即数时手动按 swizzle 挑分量再 `format_imm`。简单变量取分量走 VarRef 的 mask 路径(parser 里已合并)。
10. **资源绑定注释**:HLSLTexture/HLSLSampler 输出 `// Texture2D X bound to t50`(与 Python 一致),便于读输出。
    注意 `HLSLTexture` 是 11 字符,`substr(11)`(曾误用 10)。

## 验证方法论(改完必须验证)

1. **fxc 交叉验证**(最重要):把 HLSL 片段(含自定义函数)提取成独立 shader,
   `fxc -T ps_5_0 -E main -Od -Fc out.asm in.hlsl`,对比 fxc 和我的翻译。
   fxc 路径:`C:\Program Files (x86)\Windows Kits\10\bin\10.0.17763.0\x64\fxc.exe`
   (必须用 -Od 避免 fxc 优化掉内容)
2. **被读分量对比**:对比 mod(正常版)和我的输出,源 swizzle **前 N 位**(N=目标宽度)应一致(纯填充位无关)。
3. **结构验证**:`python tests/validate_asm.py <输出>`
4. **回归**:`bash tests/run_tests.sh`
5. **人工排查**(用户会配合):改最终输出行显示中间值(如 `mov o0.xyz, rX.xxx`),逐个看哪个值不对 —— 曾用此法定位浮点比较 bug。

## 参考源

- DXC(主,读源码):`E:\Project\Open\DirectXShaderCompiler`
  - HLSL 前端 `tools/clang/lib/`(Parse/Sema/AST),DXIL 相关 `lib/HLSL/`
  - token 规范 `include/dxc/Support/d3d12TokenizedProgramFormat.hpp`(swizzle/writemask 编码)
  - **DXC 不生成 DXBC 汇编**(输出 DXIL,<6.0 profile 提升到 6.0)
- fxc(黑盒验证,见上)
- vkd3d(辅助,swizzle 函数移植来源):`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`

## 文档

- `PROJECT_SUMMARY.md` 详细速查 / `PROJECT_NOTES.md` 进度与 bug 记录 / `tests/` 测试样例
