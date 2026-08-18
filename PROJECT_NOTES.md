# 项目笔记(给醒来后的我)

> 这是 Claude 在你睡觉期间全自主运行 C++ 重写任务时维护的活文档。
> 每完成一个重要阶段、做出关键决定、遇到问题,我都会更新这里。
> **你可以从这里了解全部进度、恢复方法、遗留问题。**

## 任务概述

把 Python 版 HLSL/DXBC 混合语法翻译器(`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`)用 C++ 重写到
`E:\Project\Cpp\hlsl_blend_dxbc_translator`,翻译/编译逻辑参考 DXC(为主)和 vkd3d(辅助)改进,不沿用 Python 版不成熟的逻辑。

## 最新状态

**会话开始时间**:2026-08-16
**当前阶段**:主光/阴影问题已修复,用户确认现有案例正常 ✅;已创建总结文件 + skill
**检查点(2026-08-17,上下文压缩后)**:与 Python 版全面对比(外部 AI 逐条核对,5 条全部属实)→ 修复绝对路径导入 + 重定义警告 + 缺失库报错列出搜索路径;snippet 内嵌标记暂不改(已给用户看法);rules.txt 确认是未使用残留
**检查点(2026-08-17,开源准备)**:补齐差距清单低风险项(矩阵报错/前置++/位复合/mad/reflect)→ 用户决定开源到 GitHub(GPL-3.0,保留历史,以后英文中性提交,对话内容只进笔记)→ 已加英文 README + GPL-3.0 LICENSE + 清理 untrack 生成的测试输出/Python rules 残留/Translator.bat(个人部署脚本)

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

### ✅ 已修复:浮点比较结果未转 0/1(用户人工排查定位的根因,影响主光/阴影全链)
- **现象**:主光/遮挡阴影光照异常,NaN 污染导致纯黑/错误
- **用户定位**:`PointLightShadow=(PointLightShadow>0.5)` 二值化处,我只有 `lt`,Python 有 `min` 保险
- **根因**(2026-08-17):DXBC **浮点比较(lt/ge/eq/ne)返回 0 或全1位(0xFFFFFFFF=NaN as float),不是 0.0/1.0**。
  NaN 参与后续数学运算,污染整个阴影/主光链。
- **修复**:fxc 标准做法,比较后加 `and dst, dst, l(0x3f800000)` 转成精确 0.0/1.0
  (fxc 对 step 即如此;比 Python 的 min 保险更标准)。
  整数比较(ilt/ige/ieq/ult/uge)已返回真 0/1,不需转换。
- **覆盖**:gen_cmp 全部浮点比较 + istep/isnan/isfinite。
- **验证**:40 处生成比较全部配对 and;回归 14/14。
- **教训**:用户人工排查(逐个改输出看中间值)是高效定位手段。

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

## 与 Python 版全面对比(2026-08-17,外部 AI 逐条核对,5 条全部属实)

对比结论:C++ 版在函数展开(作用域 vs 名称混淆)、浮点比较归一化、swizzle 一致性、控制流、类型系统上都是实质改进。
Python 有而 C++ 缺的项:

1. **snippet 内嵌 HLSLMov/DXBCMov 不支持** —— 真实功能缺口,但已确认**架构使然而非实际需求**:
   Python 的 `_process_statements` 是统一逐行处理循环(snippet/if/else/函数体共用),标记前缀判断顺手加上,天然支持;
   C++ 有真正的 HLSL Parser,snippet 内容整体交给 Parser。真实 shader 里 HLSLMov/DXBCMov 都在汇编块之间的顶层,
   一般不会混进 snippet。**用户拍板暂不改**,遇到再说。
2. **绝对路径导入函数库不支持** —— **已修复**:原来 `base + "\\" + rel` 硬拼,绝对路径必失败;
   现在检测 `盘符:` / 前导 `\`/`/`,绝对路径直接使用。
3. **函数库缺失时全盘终止** —— 保持严格终止(翻译要么对要么错),但**报错信息现在列出搜索过的候选路径**,便于定位:
   `function library not found: xxx (searched: data目录, 输入目录, .)`。
4. **函数重定义无警告** —— **已修复**:后加载的库覆盖同名函数时向 stderr 打印
   `Warning: function 'xxx' redefined by <路径>`。

附带发现(外部 AI 未报告):`Tex.SampleLevel(...).xyz` 这种"函数调用结果直接取分量"报
`member access not supported` —— 已知限制,无测试案例触发,暂不处理。

**rules.txt 确认是未使用残留**:C++ 源码没有任何地方读 rules.txt(规则系统已按决定弃用,改用内置 intrinsic 表)。
`data/rules.txt` 是从 Python 版原样复制的(与 PythonMain/rules.txt 完全一致,3226 字节)。可安全删除,留着仅作历史参考。

### 第二轮补齐(2026-08-17):绑定注释 + 表达式取分量 + rgba swizzle

- **资源绑定注释**:HLSLTexture/HLSLSampler 现在输出 `// Texture2D X bound to t50`(与 Python 完全一致,v0.5 输出 33 条)。
  顺手修了 `handle_texture` 的 `substr(10)` 潜藏 bug(`HLSLTexture` 是 11 字符,alias 因用 toks.back() 侥幸没暴露)。
- **表达式取分量 `f(...).xyz`**:修复 member access——`eval(base)` 求值成 operand 再按 swizzle mov。
  `Tex.SampleLevel(...).xyz`、`float4(1,2,3,4).w`、`(C+Swz).x` 都正确。
- **rgba swizzle 支持**:`.r/.g/.b/.a` 归一化为 `.x/.y/.z/.w`(HLSL 两种写法都合法),此前 `C.r` 会被误当 struct 成员报错。
- 回归 15/15(新增 member 测试)。

### HLSL 标准差距盘点(2026-08-17,用户问)

按实用优先级排列,均实测确认:
1. **矩阵静默出错(高风险)**:`float4x4(...)` 构造只取第一个参数当 cast,`mul(M,V)` 是分量乘非矩阵乘,
   transpose/determinant 缺失。类型能解析,但运算静默输出错值 —— 建议至少先改成报错。
2. **TextureCube/3D/2DArray 采样**:能绑定但只发 2D sample 系列(缺 sample_cube、3D UV);缺 Load/Gather/SampleCmpLevelZero。
3. **数组**:能声明+常量索引读,缺 braced 初始化、写入 arr[i]=x、动态索引。
4. **控制流/操作符**:缺 do-while、前置 ++i/--i、位复合赋值(&= |= ^= <<= >>=)、函数重载(静默覆盖)、递归(会无限展开)。
5. **内置函数缺失**:tan/asin/acos/atan/atan2、sinh/cosh/tanh、faceforward/reflect/refract/lit、mad/modf/frexp/ldexp/isinf、
   reversebits/countbits/firstbithigh/firstbitlow、fwidth/ddx_coarse/fine/ddy_coarse/fine、clip、transpose/determinant。
6. **struct 完全不支持**;预处理器/cbuffer/属性在 3Dmigoto 标记格式中一般不出现,影响小。

### 第三轮补齐(2026-08-17):矩阵报错 + 前置++ + 位复合赋值 + mad/reflect

按用户授权(改动小/风险低/价值高),从差距清单中挑的这批:
- **矩阵改为报错**(防静默错误,最高优先):`float4x4(...)` 构造、`(float4x4)x` cast、`float4x4 M` 声明、HLSLMov 矩阵声明
  全部输出 `matrix types not supported`,不再静默输出错值。
- **前置 ++i/--i**:降级为 `i+=1`/`i-=1`(与后置一致)。
- **位复合赋值** `&= |= ^= <<= >>=`:lexer 补 `&= |= ^=` 三个 token,parser 识别后走 CompoundAssign。
- **内置函数 mad/reflect**:`mad(a,b,c)`→mad 指令;`reflect(I,N)=I-2·dot(N,I)·N`→dp3+add+mul+add。
  fxc 交叉验证:mad 完全一致,reflect 语义一致(fxc 用 dot(V,N),点积可交换)。
- 回归 17/17(新增前置++/位复合/mad/reflect、矩阵报错测试)。

### C++ 超出 Python 的能力(用户问,2026-08-17)

- 类型:int/uint/bool + 位运算(& | ^ << >> ~)、整数比较、utof、asfloat/asint/asuint
- 控制流:for/while(loop/breakc/endloop)、switch/case/default、discard、break/continue(Python 只有 if/else)
- 运算:复合赋值、i++/i--、常量折叠
- 采样:SampleLevel/SampleCmp/SampleBias/SampleGrad(Python 只有 sample)
- intrinsic:53 个,新增 any/all、ddx/ddy、rcp、radians/degrees、isnan/isfinite、fmod、sign、select、round/floor/ceil/trunc/frc、distance、rsqrt 等(Python 只有 rules.txt 约 25 个)
- 浮点比较 fxc 标准归一化、函数展开作用域隔离(优于 Python 名称混淆)

## 会话分工(2026-08-17,用户确认)

- **本对话 = 翻译器中枢**:改 bug、加功能、验证输出都在这里。
- **mod 编写单开对话**:新对话读 PROJECT_SUMMARY/SKILL/PROJECT_NOTES/README 后写 HLSLBlend。
- 出问题(翻译器报错/输出可疑)→ 截最小复现片段回本对话定位修复。

## lilToon 复刻 mod 需求 + 技术验证(2026-08-17)

用户要在 DMC5 用本翻译器复刻 Unity lilToon 效果(风格化渲染),作为翻译器实战测试。
需求来自另一侧 Agent 分析(lilToon 函数统计)+ 我用 fxc/3Dmigoto 源码实测确认:

**fxc + 3Dmigoto 源码实测结论(E:\Project\Open\3Dmigoto, D3D_Shaders/Assembler.cpp 指令表)**:

| 功能 | 3Dmigoto 接受的指令 | 实现 |
|---|---|---|
| clip(x) | `discard_nz` | `lt t,x,l(0)` + `discard_nz t`(fxc 实测如此;**不需要** and l(0x3f800000),discard_nz 只测非零) |
| 无条件 discard 语句 | `discard_nz` | **现有 bug**:发 `discard`,3Dmigoto 指令表无此名 → 改 `discard_nz l(-1)`(fxc 实测生成) |
| ddx/ddy | `deriv_rtx`/`deriv_rty` | **现有 bug**:发 `ddx`/`ddy`,3Dmigoto 指令表无此名 → 必须改 |
| fwidth(x) | 无指令,展开 | `deriv_rtx` + max(abs) + `deriv_rty` + max(abs) + add(fxc 实测 7 指令) |
| mul(M, v) | dp3/dp4 序列 | M 每行 × v(行 i = 基寄存器 + i),fxc 实测确认 |
| mul(v, M) | dp3 序列 | v × M 每列(列优先存储下 = 跨寄存器取第 j 分量组装) |
| (float3x3)x cast | — | float4x4→3x3 取前 3 行 |
| Texture2DArray 采样 | `sample_c_lz`(非 indexable 变体) | 3 分量 UV(xy+index);资源维度靠透传的原版 dcl_resource |
| sample 简化格式 | 支持 | 3Dmigoto 注释明确:指令表含非 indexable 变体;M2 部署已验证 |

**已确认 3Dmigoto 汇编器支持**:非 indexable 的 sample/sample_c/sample_c_lz/sample_l/sample_d/sample_b、
deriv_rtx/deriv_rty、discard_nz/discard_z、if_nz/if_z、switch、movc 等。

**优先级**:1) 修 ddx/ddy + discard 指令名 bug(影响已发布代码) → 2) clip → 3) fwidth → 4) 矩阵(最大,核心需求) → 5) SampleCmpLevelZero/2DArray。
注意回归是 **12 项**(非另一侧 Agent 说的 17 项)。矩阵语义(cbuffer 列优先)用 fxc 交叉验证防错。

**另一侧 Agent 确认后的最终优先级(2026-08-17)**:
- P0: ddx/ddy→deriv_rtx/deriv_rty、discard→discard_nz(现有 bug)
- P0: clip(x)(透明裁剪)
- P0: mul(M,v) + mul(v,M) + (float3x3)cast —— lilToon 核心,mul(v,M) 确认 9 处高频使用
- P1: fwidth(AA 关键)
- P2: Texture2DArray 普通采样(3 分量 UV,可选)—— lilToon 的 LIL_SAMPLE_2D_ARRAY 用普通 Sample 非 SampleCmp
- 跳过: SampleCmpLevelZero(lilToon 0 次)、矩阵×矩阵(0 次)、GetDimensions(尺寸外传/固定值绕过)
- 后补: transpose(lilToon 仅 1 处,可拆手动交换索引)
- 注意:lilToon 矩阵来自 Unity(float4x4),用户需在 HLSLBlend 里绑定游戏 cbuffer 的对应矩阵;
  矩阵实现要支持 cbuffer 基寄存器(如 cb0[0])。

### lilToon 支持实现记录(2026-08-17,已提交)

已完成:指令名修正(ddx/ddy→deriv_rtx/deriv_rty、discard→discard_nz l(-1))、clip、fwidth、矩阵 mul + cast。
回归 14/14。**矩阵列优先语义是核心(务必遵守)**:

- **cbuffer 列优先存储**:float4x4 M 绑定 cb0[0],则 base[j] 是 M 的**第 j 列**(不是行)。
- `mul(M, v)`(矩阵×列向量):输出 i = dot(M 第 i 行, v)。第 i 行 = **跨每个寄存器的第 i 分量**
  (cb0[0][i], cb0[1][i], cb0[2][i]),需先 mov 组装临时再 dp3。fxc 反汇编证实。
- `mul(v, M)`(行向量×矩阵):输出 j = dot(v, M 第 j 列) = **直接用 base[j] 寄存器** dp3。
- 之前把两者实现写反了(把列当行),fxc 交叉验证才发现。
- **矩阵变量占 rows 个连续寄存器**:HLSLMov float4x4 M = cb0[0] 绑定 base+rows(不发 mov);
  snippet 里 float3x3 M 声明分配连续临时(alloc_matrix)+ 清零。
- **寄存器保留**:矩阵绑定后 reserve_regs(base, rows),临时分配跳过,避免后续临时撞矩阵行。
- **reg_plus 坑**:rN 行偏移曾输出 "r55"(前缀带数字),应只保留非数字前缀("r"+数字)。
- float4x4(...) 构造、matrix×matrix 仍报错;transpose 后补。ddx/ddy 在 3Dmigoto 是 deriv_rtx/deriv_rty(coarse 默认,无 _coarse 后缀写法)。

## 参考位置备忘

- DXC:`E:\Project\Open\DirectXShaderCompiler`
- vkd3d:`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`
- 测试样例:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\Test\`
