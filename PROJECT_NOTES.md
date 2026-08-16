# 项目笔记(给醒来后的我)

> 这是 Claude 在你睡觉期间全自主运行 C++ 重写任务时维护的活文档。
> 每完成一个重要阶段、做出关键决定、遇到问题,我都会更新这里。
> **你可以从这里了解全部进度、恢复方法、遗留问题。**

## 任务概述

把 Python 版 HLSL/DXBC 混合语法翻译器(`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`)用 C++ 重写到
`E:\Project\Cpp\hlsl_blend_dxbc_translator`,翻译/编译逻辑参考 DXC(为主)和 vkd3d(辅助)改进,不沿用 Python 版不成熟的逻辑。

## 最新状态

**会话开始时间**:2026-08-16
**当前阶段**:核心管线完成 + 功能增强中,已验证格式兼容性 ✅

阶段进度:
- [x] 通读 Python 源码、规则、测试样例;确认构建环境与 DXC 位置
- [x] 搭建 C++ 项目骨架(src/ 模块化、vcxproj、MSBuild)
- [x] 核心模型(types/swizzle/AST)、词法+语法解析器、符号表+寄存器分配
- [x] intrinsic 表(~50 内置函数)+ codegen、translator 主循环(全部 8 标记)
- [x] 函数库展开、纹理采样、if/else/while/for 控制流
- [x] 三个真实 shader(M2全局光照/星见雅modx2)零错误翻译
- [x] **格式验证**:49 个生成助记符全部在真实 3Dmigoto 反汇编集合内;操作数结构验证器全通过
- [x] **健壮性**:56 个 ShaderFixes + 604 个 ShaderCache 文件零崩溃;纯 DXBC 透传逐字节一致
- [x] **功能增强**:int/uint 位运算(& | ^ << >> + 整数立即数位模式)、int 比较指令(ilt/ige/ieq)、
      uint→float 用 utof、any()/all()、discard、i++/i--、SampleLevel(sample_l)、常量折叠
- [ ] 待做:switch 语句、sample_c(深度比较采样)、更多 intrinsic、向量动态索引
- [ ] **最终验证(需用户)**:在 DMC5 游戏中实际加载测试(3Dmigoto 汇编器没有独立 CLI,
      我已通过格式对比+结构验证确认兼容性,但游戏内加载是最权威验证)

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

(暂无 —— 自主运行期间遇到会记录在这里)

## 参考位置备忘

- DXC:`E:\Project\Open\DirectXShaderCompiler`
- vkd3d:`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`
- 测试样例:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\Test\`
