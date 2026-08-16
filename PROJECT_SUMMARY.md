# 项目总结(上下文压缩后的速查手册)

> 本文浓缩整个项目从零到当前状态的关键内容。上下文被压缩后,先读这个 + `PROJECT_NOTES.md`。
> 最后更新:2026-08-17(主光/阴影问题已修复,待用户游戏内最终确认)

## 一、项目是什么

用 **C++17** 重写 Python 版 **HLSL/DXBC 混合语法翻译器**。目标:给游戏 shader 修改(3Dmigoto ShaderFixes)用。

- **输入**:DXBC sm5 汇编 + HLSL 混合标记
- **输出**:纯 DXBC sm5 汇编文本
- 项目根:`E:\Project\Cpp\hlsl_blend_dxbc_translator`

## 二、输入标记语法(8 种,保持不变)

| 标记 | 作用 |
|---|---|
| `HLSLMov <类型> <变量>[.swizzle] = <寄存器>;` | 把 HLSL 变量绑定到 DXBC 寄存器 |
| `DXBCMov <目标寄存器>, <变量>` | 把 HLSL 变量写回 DXBC 寄存器 |
| `HLSL <单行 HLSL 语句>` | 翻译单行语句 |
| `HLSLSnippet { ... }` | 翻译一段 HLSL(支持 if/else/for/while/switch) |
| `HLSLInit` | 标记:所有分配过的临时寄存器在此清零 |
| `HLSLTexture Texture2D X = tN;` | 绑定纹理别名 |
| `HLSLSampler SamplerState X = sN;` | 绑定采样器别名 |
| `HLSLFunctionImport "functions/lib.txt";` | 导入自定义函数库 |

其他行**原样透传**。`//` 注释保留在透传行。

## 三、架构(src/ 模块)

```
main.cpp        CLI: -input/-output/-data/-no-sat-fold
types.h/cpp     HLSL 类型模型(scalar/vector/matrix + base type)
swizzle.h/cpp   4分量swizzle编码(2bit×4),writemask转换
ast.h           Expr/Stmt 节点(Arena 分配)
lexer.h/cpp     词法
parser.h/cpp    递归下降(表达式+语句+控制流)
symbols.h/cpp   符号表(作用域栈)+ 寄存器分配(free list)
intrinsics.h/cpp DXC风格集中式 intrinsic 表(~65 函数)
codegen.h/cpp   AST→DXBC 指令(类型推断、操作数格式化、saturate折叠)
translator.h/cpp 主循环(8 标记)+ 语句翻译 + 控制流 + 函数展开
function_lib.h/cpp lib.txt 函数库解析
```

## 四、构建 / 测试 / 运行

```bash
# 构建(MSBuild Release x64)
"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" hlsl_blend_dxbc_translator.sln /p:Configuration=Release /p:Platform=x64

# 回归测试(14 项,含 6 个真实 shader + 特性测试 + 结构验证)
bash tests/run_tests.sh

# 运行
x64\Release\hlsl_blend_dxbc_translator.exe -input <混合文件> -output <输出> -data data
# -data data 指定函数库目录;另有 -no-sat-fold(禁用 saturate 折叠,用 min/max,排查用)
```

也可用 CMake/MinGW 构建。测试样例在 `tests/`(从 Python 项目复制)。

## 五、最关键的经验(踩过的坑,务必牢记)

### 1. DXBC 浮点比较返回 0 或 NaN,不是 0/1(最重要的坑)
`lt/ge/eq/ne` 返回 **0.0 或全 1 位模式(0xFFFFFFFF,解释为 float 是 NaN)**,不是 0.0/1.0!
**必须加 `and dst, dst, l(0x3f800000)` 归一化**(fxc 标准做法;0x3f800000 = float 1.0 位)。
整数比较(`ilt/ige/ieq/ine/ult/uge`)已返回真 0/1,不需要。
**这是主光/阴影异常的根因** —— NaN 参与数学运算污染全链。
已在:`gen_cmp` 全部浮点比较 + `istep`/`iisnan`/`iisfinite`/`isign` + 逻辑非 `!`(eq)。

### 2. saturate 折叠只折叠最外层
`saturate(a*b*c)` 应生成 `mul(a,b); mul_sat(×c)`(单层)。
**必须保证内层子表达式(算进临时寄存器)不继承 `_sat`** —— 在 `CodeGen::eval` 的复杂表达式分支临时关闭 `fold_sat_`。
否则会变 `saturate(saturate(a*b)*c)`,结果错误(主光高光被削弱)。

### 3. sincos 参数映射
HLSL `sincos(angle, out_sin, out_cos)` → DXBC **`sincos <sin>, <angle>, <cos>`**(sin→第2参数,cos→第3参数)。
注意:Python 版用互换(sin→第3参数)也能跑(黄金角螺旋不敏感),但**标准是 sin→第2参数**。

### 4. 寄存器分配:作用域 + 空闲列表复用
- 临时寄存器用后即还(free list)
- 函数局部变量作用域结束释放
- **注意复用冲突**:变量和临时不要生命周期重叠(用不同寄存器)

### 5. 函数局部变量遮蔽
`translate_decl` 必须用 `lookup_current_scope`(只查当前作用域),否则函数内局部变量会和外部同名变量别名(共用寄存器),导致错误。

### 6. swizzle 填充:复制第一个有效分量
`xy→xyxx`、`xyz→xyzx`、`zw→zwzz`、`zyx→zyxz`(匹配 fxc/3Dmigoto 惯例)。
不要用"复制最后一个"(replicate-last)。

### 7. HLSLInit
清零**所有分配过的寄存器**(含已释放后复用的),避免脏数据。

## 六、验证方法(按可靠性排序)

1. **游戏内测试**(3Dmigoto 加载 ShaderFixes)—— 最权威,需用户配合
2. **fxc 交叉验证**:把 HLSL 片段提取成独立 shader(含自定义函数),`fxc -T ps_5_0 -E main -Od -Fc out.asm in.hlsl`,对比 fxc 和我的翻译(注意 fxc 会优化,用 -Od 保留)
3. **被读分量对比**:对比 mod(正常版)和我的输出,源 swizzle 的**前 N 位**(N=目标宽度)应一致(纯填充位无关)
4. **结构验证**:`python tests/validate_asm.py <输出>`
5. **人工排查**:改最终输出行,把中间值直接显示(如 `mov o0.xyz, rX.xxx`),逐个看哪个值不对 —— 用户就是这么定位浮点比较 bug 的

## 七、参考源

- **DXC**(主参考,读源码):`E:\Project\Open\DirectXShaderCompiler`
  - HLSL 前端在 `tools/clang/lib/`(Parse/Sema/AST),DXIL 相关在 `lib/HLSL/`
  - 微软 token 规范:`include/dxc/Support/d3d12TokenizedProgramFormat.hpp`(swizzle/writemask 编码)
  - DXC **不生成 DXBC 汇编**(输出 DXIL,<6.0 profile 提升到 6.0)
- **fxc**(微软 DXBC 编译器,闭源,黑盒验证):`C:\Program Files (x86)\Windows Kits\10\bin\10.0.17763.0\x64\fxc.exe`
- **vkd3d**(辅助):`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`(swizzle 函数移植来源)
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`

## 八、当前状态

- 核心翻译器完成,~65 个 intrinsic,MSVC+MinGW 双构建,14 项回归通过
- 已修复:浮点比较 NaN(根因)、saturate 嵌套折叠、sincos 参数、函数局部遮蔽、swizzle 惯例、显式 swizzle 保留
- **待办**:
  - 用户游戏内最终测试确认主光/阴影正常
  - 清理 tests/ 里用户加的调试代码(`Test`/`r200`/`Test.x = PointLightShadow`)
  - 已知限制:向量动态索引、矩阵表达式、atan2 等复杂三角未实现
- 活文档:`PROJECT_NOTES.md`(详细进度、每个 bug 的修复记录)
- 持久记忆:`C:\Users\wk135\.claude\projects\E--Project-Cpp-hlsl-blend-dxbc-translator\memory\`
