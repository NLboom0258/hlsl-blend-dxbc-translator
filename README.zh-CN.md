# HLSL Blend DXBC Translator

[English](README.md) | [中文](README.zh-CN.md)

用 **C++17** 编写的翻译器:把用 **DXBC SM5 汇编文本 + 内嵌 HLSL 片段**混合编写的着色器,
翻译回**纯 DXBC SM5 汇编文本**。

DXBC 汇编格式是微软 HLSL 着色器编译器(fxc)的标准反汇编输出——3Dmigoto 等 shader 修改工具
用的都是这种格式。混合文件就是普通的 DXBC 汇编里穿插几个 HLSL 标记,这样原本在裸汇编里
写起来很麻烦的数学运算可以直接用 HLSL 写。本工具解析这些文件,把每个 HLSL 片段翻译成
正确的 DXBC SM5 指令。

本项目由 AI Agent 深度参与编写,大部分代码由 AI 生成。它是对作者此前用 Python
编写的原型的 C++ 重构——那个 Python 原型定义了工具的输入/输出格式和行为。
翻译逻辑参考官方编译器——以 DXC(`DirectXShaderCompiler`)为主、vkd3d 为辅——以保证正确性。

## 8 种标记语法

非标记行原样透传。标记会被翻译:

| 标记 | 作用 |
|---|---|
| `HLSLMov <类型> <变量>[.swizzle] = <寄存器>;` | 把 HLSL 变量绑定到 DXBC 寄存器 |
| `DXBCMov <目标寄存器>, <变量>[.swizzle]` | 把 HLSL 变量写回 DXBC 寄存器 |
| `HLSL <单行 HLSL 语句>` | 翻译单行 HLSL 语句 |
| `HLSLSnippet { ... }` | 翻译一段 HLSL(支持 if/else/for/while/switch) |
| `HLSLInit` | 在此处清零所有分配过的临时寄存器 |
| `HLSLTexture Texture2D X = tN;` | 绑定纹理别名 |
| `HLSLSampler SamplerState X = sN;` | 绑定采样器别名 |
| `HLSLFunctionImport "functions/lib.txt";` | 导入自定义函数库 |

示例:

```
HLSLTexture Texture2D IlmMapTex0 = t50;
HLSLSampler SamplerState IlmMapSampler0 = s13;

HLSLSnippet {
    float3 col = IlmMapTex0.SampleLevel(IlmMapSampler0, uv, 0).xyz;
    float3 n = normalize(col);
    col = saturate(n * dot(n, LightDir));
}
```

## 构建

### MSBuild(Visual Studio 2022, v143)

```
MSBuild.exe hlsl_blend_dxbc_translator.sln -p:Configuration=Release -p:Platform=x64
```

### CMake / MinGW

项目也可以用 CMake 和 MinGW g++(C++17)构建。

## 用法

```
hlsl_blend_dxbc_translator.exe -input <文件> [-output <输出.asm>] [-data <函数库目录>]
```

- `-data` 指定函数库查找目录(用于 `HLSLFunctionImport` 相对路径)。查找顺序:`-data` 目录 →
  输入文件所在目录 → 当前目录。没有默认函数库——只有 `HLSLFunctionImport` 标记引用的文件才会被加载。
- `-no-sat-fold` 禁用 saturate 折叠(改用 min/max)——排查用调试参数。

## 函数库

本翻译器没有"内置标准库"。所有 HLSL 指令和语法都是内置的;`HLSLFunctionImport`
只是自定义函数的扩展机制,不是必需依赖。

`data/functions/lib.txt` 是一个**参考示例**,不是标准库。其中的函数是为特定 mod
配套编写的——即 tests/ 里的 M2 全局光照 v0.5 测试 shader,由那个 shader 使用。它们
写于早期翻译器时期,当时语法支持有限,代码比较粗糙(例如 ID 切换是硬编码的 if/else
嵌套)。把它当作编写自定义函数库的语法参考即可,不建议直接照搬使用。

## 支持的 HLSL

- **类型**:`float`/`int`/`uint`/`bool`/`double`/`half`/`min16*`、向量和 4x4 以内矩阵。
  矩阵运算已支持(`mul(M,v)`、`mul(v,M)`、`(float3x3)` cast),按**列优先**语义,fxc 验证
- **完整控制流**:`if/else`、`for`、`while`、`switch/case`、`discard`、`break`、`continue`,
  导入函数内任意位置 `return`
- **int/uint 位运算**、复合赋值(`+=` `&=` `<<=` ...)、前置/后置 `++/--`
- **采样**:`Sample`、`SampleLevel`、`SampleCmp`、`SampleBias`、`SampleGrad`
- **约 59 个内置函数**,包括 `normalize`、`smoothstep`(基于 mad,与 fxc 一致)、`mad`、
  `reflect`、`clip`、`fwidth`、`any/all`、`ddx/ddy`、`fmod`、`radians/degrees` 等
- **fxc 标准的浮点比较**(`lt/ge/eq/ne` 用 `and dst, dst, l(0x3f800000)` 归一化——
  DXBC 浮点比较返回 0 或全 1 位/NaN,不是 0.0/1.0,这是标准修法)

## 验证

- `bash tests/run_tests.sh` — 12 项回归测试(真实 mod shader + 特性测试 + 汇编结构验证)
- **fxc 交叉验证**:把 HLSL 片段(含导入函数)用 `fxc -T ps_5_0 -E main -Od -Fc out.asm in.hlsl`
  编译,与翻译器输出对比
- 与已知正确的参考输出做被读分量对比
- `python tests/validate_asm.py <输出>` — 结构验证

## 已知限制

TextureCube/3D 采样(`sample_cube`)、数组写入/动态索引、`do-while`、`struct`、`transpose`、
`mul(M1, M2)`(矩阵×矩阵)、以及少数冷门内置函数(`atan/asin/acos/tan` 级数展开等)尚未实现。
完整差距清单记录在项目笔记里。

## 参考

- **DXC** — 主参考:`https://github.com/microsoft/DirectXShaderCompiler`
- **vkd3d** — 辅助参考:`https://gitlab.winehq.org/wine/vkd3d`
- **3Dmigoto** — 使用这种汇编格式的 shader 修改工具

## 许可证

GPL-3.0。见 [LICENSE](LICENSE)。
