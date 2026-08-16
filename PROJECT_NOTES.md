# 项目笔记(给醒来后的我)

> 这是 Claude 在你睡觉期间全自主运行 C++ 重写任务时维护的活文档。
> 每完成一个重要阶段、做出关键决定、遇到问题,我都会更新这里。
> **你可以从这里了解全部进度、恢复方法、遗留问题。**

## 任务概述

把 Python 版 HLSL/DXBC 混合语法翻译器(`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`)用 C++ 重写到
`E:\Project\Cpp\hlsl_blend_dxbc_translator`,翻译/编译逻辑参考 DXC(为主)和 vkd3d(辅助)改进,不沿用 Python 版不成熟的逻辑。

## 最新状态

**会话开始时间**:2026-08-16
**当前阶段**:核心翻译管线已完成,三个真实 shader 零错误翻译 ✅

阶段进度:
- [x] 通读 Python 源码(translator / ast_nodes / symbol_table / rule_loader / rule_engine / function_loader / utils / main)
- [x] 通读 rules.txt、functions/lib.txt、测试样例
- [x] 确认构建环境(VS2022 v143 + MSBuild)、DXC 克隆位置
- [x] 写入 CLAUDE.md(我的工作准则)与本笔记
- [x] 搭建 C++ 项目骨架(src/ 模块化、vcxproj 通配符、MSBuild 编译通过)
- [x] 核心模型:types(类型系统)、swizzle(vkd3d 两步 swizzle)、AST
- [x] 词法+语法解析器(完整表达式优先级、语句、控制流、常量折叠)
- [x] 符号表+寄存器分配(作用域栈、free list、ever-allocated 跟踪)
- [x] intrinsic 表 + codegen(DXC 风格集中式表,~45 个内置函数)
- [x] translator 主循环(全部 8 种标记、if/else/while/for、注释剥离、HLSLInit)
- [x] 函数库解析与展开(HLSLFunctionImport、in/out/inout、作用域)
- [x] 纹理采样(Texture2D/Cube/Array、HLSLTexture/HLSLSampler 绑定)
- [x] 三个真实 shader 零错误翻译(M2全局光照/星见雅modpbr/星见雅mod身体)
- [ ] 类型/intrinsic 增强(int/uint/bool 位运算等)、循环已在基础层面支持
- [ ] 汇编器验证输出正确性
- [ ] 对比 Python 输出、检查翻译质量
- [ ] 用 DMC5 ShaderFixes/ShaderCache 真实样例扩展测试

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
  hlsl_blend_dxbc_translator.exe -input <HLSLBlend文件> -output <输出文件>
  ```
- **测试**:测试样例从 Python 项目 Test 目录复制到本项目 tests/。
- **对比 Python 输出**:Python 版跑法:`cd E:\Project\Python\hlsl_blend_dxbc_translator && python PythonMain/main.py -input <文件> -output <输出>`

## 遇到的问题 / 待用户决定

(暂无 —— 自主运行期间遇到会记录在这里)

## 参考位置备忘

- DXC:`E:\Project\Open\DirectXShaderCompiler`
- vkd3d:`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`
- 测试样例:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\Test\`
