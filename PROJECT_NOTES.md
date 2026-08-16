# 项目笔记(给醒来后的我)

> 这是 Claude 在你睡觉期间全自主运行 C++ 重写任务时维护的活文档。
> 每完成一个重要阶段、做出关键决定、遇到问题,我都会更新这里。
> **你可以从这里了解全部进度、恢复方法、遗留问题。**

## 任务概述

把 Python 版 HLSL/DXBC 混合语法翻译器(`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`)用 C++ 重写到
`E:\Project\Cpp\hlsl_blend_dxbc_translator`,翻译/编译逻辑参考 DXC(为主)和 vkd3d(辅助)改进,不沿用 Python 版不成熟的逻辑。

## 最新状态

**会话开始时间**:2026-08-16
**当前阶段**:核心完成 + 功能增强 + 全面验证 ✅(等待用户游戏内最终测试)

阶段进度:
- [x] 通读 Python 源码、规则、测试样例;确认构建环境与 DXC 位置
- [x] 搭建 C++ 项目骨架(src/ 模块化、vcxproj、MSBuild + CMake 双构建)
- [x] 核心模型(types/swizzle/AST)、词法+语法解析器、符号表+寄存器分配
- [x] intrinsic 表(~60 内置函数)+ codegen、translator 主循环(全部 8 标记)
- [x] 函数库展开、纹理采样、if/else/while/for/switch 控制流
- [x] 三个真实 shader 零错误翻译;56 ShaderFixes + 604 ShaderCache 零崩溃
- [x] **生产验证**:用户实际部署的 mod shader(二次元卡通渲染风格shader替换Mod/ShaderFixes/695c8c0feada9292-ps.txt,
      1839 行/12 snippets)零错误翻译 + 结构验证通过
- [x] 格式验证:助记符全在 3Dmigoto 集合内 + 结构验证 + **fxc 交叉验证**(normalize/dot/saturate/ternary/clamp/exp/log/sincos/cross 与微软编译器一致)
- [x] 功能增强(详见下方「扩展语法」)
- [x] 可移植性:MSVC + MinGW/g++ 双编译器构建通过
- [x] 健壮性:500 次随机模糊测试 0 崩溃
- [x] **与 Python 版逐指令对比**(2026-08-17):1142/1143 个源操作数的被读分量映射与 Python 完全一致;
      唯一差异(3分量源→4分量目标)的受影响分量(w)未被读取,无语义影响
- [x] swizzle 填充改为"复制第一个有效分量"(用户指正:.xy→.xyxx/.xyz→.xyzx/.zw→.zwzz)
- [x] **fxc 铁证确认**(2026-08-17):fxc 对 `float2 uv` 采样输出 `v0.xyxx`(复制第一个),与我的实现一致;
      Python 的 `xyyy` 是 Python 特有(replicate-last),与编译器不符 —— 以编译器为准
- [x] 保留用户显式 swizzle 的未使用分量(不再清零为 x;如 `r2.yzwy` 原样输出)
- [ ] **最终验证(需用户)**:在 DMC5 游戏中实际加载(3Dmigoto 汇编器无独立 CLI,已用格式对比+fxc 交叉验证确认兼容性,但游戏内加载是最权威测试)

## 扩展语法(相比 Python 版新增,用户可放心使用)

Python 版只支持 float/float2/3/4 + 基本运算。C++ 版扩展了:
- **类型**:int/uint/bool(含位运算 & | ^ << >> ~、整数比较 ilt/ige/ieq、uint→float 用 utof)
- **控制流**:for/while 循环(loop/breakc/endloop)、switch/case/default、discard、break/continue
- **运算**:i++/i--、后置自增、常量折叠(负号/构造向量/整数幂)
- **采样**:SampleLevel(sample_l)、SampleCmp(sample_c, 深度比较)、SampleBias(sample_b)、SampleGrad(sample_d)
- **intrinsic 补充**:any()/all()、ddx/ddy、rcp、radians/degrees、isnan/isfinite、fmod、sign、select、round/floor/ceil/trunc/frc
- **质量优化**:smoothstep 用 mad(仿 fxc)、pow 常量整数幂展开、**saturate 折叠到前序指令**
      (mul_sat/add_sat/dp3_sat,比较直接输出,无法折叠的回退 mov_sat —— M2 输出 1721→1689 行)

## 关键决定(用户已知晓/授权)

1. **弃用 rules.txt 规则引擎**,改为 C++ 内置 intrinsic(DXC 风格)。保留函数库机制(HLSLFunctionImport / lib.txt)。
2. **参考以 DXC 为主**(`E:\Project\Open\DirectXShaderCompiler`,用户自行克隆),vkd3d 为辅助。
3. **权限**:用户已设为最高(自动接受所有申请),睡觉期间全自主运行。
4. **git**:授权阶段性提交,尽量保证可编译。

## 怎么恢复 / 怎么查看结果

- **进度**:看本文件的「最新状态」。
- **代码**:`git log --oneline` 查看提交记录;每个阶段是一个提交。
- **构建**:在 `E:\Project\Cpp\hlsl_blend_dxbc_translator` 用 MSBuild:
  ```
  "/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" hlsl_blend_dxbc_translator.sln /p:Configuration=Release /p:Platform=x64
  ```
- **运行**:
  ```
  x64\Release\hlsl_blend_dxbc_translator.exe -input <HLSLBlend文件> [-output <输出文件>] [-data <函数库目录>]
  ```
  `-data` 缺省时依次查找:data 目录 → 输入文件目录 → 当前目录。函数库默认 `functions/lib.txt`。
- **回归测试**:`bash tests/run_tests.sh`(6 个真实 shader + 特性测试 + 结构验证)。
- **对比 Python 输出**:`cd E:\Project\Python\hlsl_blend_dxbc_translator && python PythonMain/main.py -input <文件> -output <输出>`

## 遇到的问题 / 待用户决定

### ✅ 已修复:sincos 参数错位(用户二次测试:有遮挡阴影的光照无法影响主光)
- **现象**:攻击光照/无遮挡阴影的光照能正常参与主光;场景里有正常遮挡阴影的光照无法影响角色主光。
- **根因**(2026-08-17):`isincos` 把 HLSL `sincos(angle, out_sin, out_cos)` 的参数映射错乱,
  dst_sin 写到了角度寄存器,src 读了未初始化的 offset。
- **影响链**:OffsetShadowUV 黄金角螺旋偏移错误 → 阴影采样 UV 错 → 方向光遮挡阴影因子
  (r4.y → DirLightSceneShadow)错误 → 有遮挡阴影的光照主光分支失效。
- **修复**:输出 `sincos <arg2>, <arg0>, <arg1>`(dst_sin=arg2, dst_cos=arg1),
  与 mod(Python 规则,游戏已验证正常)输出完全一致。黄金角螺旋对 sin/cos 互换不敏感。
- **验证**:我的 sincos 与 mod 结构一致;被读分量 1162/1163 一致;回归 14/14。

### ✅ 已修复:saturate 折叠 bug(用户游戏内测试发现主光消失)
- **现象**:游戏内主光不可见,只剩阴影色/局部光叠色/边缘光
- **根因**(2026-08-17):`saturate(a*b*c)` 被折叠成 `saturate(saturate(a*b)*c)`。
  `gen_fold_saturate` 设置的 `fold_sat_` 在递归翻译内层子表达式时未关闭,导致内层 `mul` 也加了 `_sat`。
- **影响**:主光 `Speculer_Color = saturate(LightColor*intensity*ilmMap.x)`,当 FinalLight>1 时被 IlmMap.x 错误截断,主光高光削弱。
- **修复**:`CodeGen::eval` 计算临时寄存器时临时关闭 `fold_sat_`,只让最外层指令带 `_sat`。
- **验证**:主光 Speculer_Color 与 mod(没问题版)语义一致;被读分量 1154/1155 一致;回归 14/14。
- **教训**:saturate 折叠这类"传播状态"的优化,必须确保状态只影响最外层指令。

### ⚠️ 你的 shader 文件里有潜在 bug(翻译器正确报告,未崩溃)
- **文件**:`强制开启接触阴影的关卡全局光照shader88d1a2e189df03f6-ps.txt` 第 550 行
- **问题**:声明的是 `OffsetDirShadowUV1`(第 549 行 `HLSL float2 OffsetDirShadowUV1 = OffsetShadowUV(...)`),
  但紧接着 `DXBCMov r16.xy, OffsetDirShadowUV2` 用的是 `OffsetDirShadowUV2` —— 这个变量从未声明(疑似 UV1/UV2 复制粘贴笔误)。
- **我的翻译器行为**:与 Python 版一致,该行输出 `// Error: DXBCMov: variable not defined: OffsetDirShadowUV2` 注释,其余继续翻译,不崩溃。
- **建议**:如果这是笔误,把 `OffsetDirShadowUV2` 改成 `OffsetDirShadowUV1`,或补一个 `HLSLMov float2 OffsetDirShadowUV2 = ...`。改完后这行就能正确翻译。
- (Python 版同样会在此报错,这是原文件的问题,不是 C++ 版引入的。)

## 参考位置备忘

- DXC:`E:\Project\Open\DirectXShaderCompiler`
- vkd3d:`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`
- 测试样例:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\Test\`
