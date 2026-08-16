# HLSL Blend DXBC Translator — C++ 重写项目

用 C++ 重写 [E:\Project\Python\hlsl_blend_dxbc_translator](E:\Project\Python\hlsl_blend_dxbc_translator)(Python 版 HLSL/DXBC 混合语法翻译器)。
输入是 DXBC 汇编 + HLSL 混合标记文件,输出纯 DXBC sm5 汇编。最终目标是给游戏 shader 修改(3Dmigoto ShaderFixes)使用。

## 工作准则(每次会话必须遵守)

1. **工具安全(最高优先级)**
   - 绝不执行不可逆/破坏性操作:无授权的 `git push --force`、`git reset --hard`、`rm -rf`、删除分支、杀进程、清数据库等。
   - 绝不改动系统级配置:环境变量、注册表、防火墙、系统服务、`C:\Users\wk135\.claude\settings.json` 全局配置、VS/IDE 配置等。
   - 只动本翻译器项目相关的文件;工作范围外文件只读,不删不改。
   - 有风险的操作先想清楚后果;不确定就写进 PROJECT_NOTES.md 留给用户决定,不要冒险。
   - 注意 git 操作范围:只在 `E:\Project\Cpp\hlsl_blend_dxbc_translator` 仓库内操作,不 push 远程。

2. **git 提交**:用户已授权在阶段性成果时提交,提交前尽量确认能编译(至少不引入编译错误)。每个阶段留下干净、可用的提交。不提交未完成/无法编译的半成品(除非用户明确要求)。

3. **模型限制**:实际模型是 DeepSeek-v4-flash(经 CCSwitch 路由),**无多模态能力,不读取/分析图片**。本项目是纯文本,一切验证用文本方式(运行程序、读输出、diff)。

4. **参考编译器**:以 DXC 为主参考(`E:\Project\Open\DirectXShaderCompiler`,用户自行克隆的完整仓库,可读);vkd3d-1.19 为辅助(`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`)。翻译/编译逻辑按编译器做法,不沿用 Python 版不成熟的逻辑。

5. **规则系统决策**:已决定弃用 Python 版的 rules.txt 数据驱动规则引擎,改为 C++ 内置的 intrinsic 实现(DXC 风格,集中式 intrinsic 表,方便增改)。**保留**函数库机制(`HLSLFunctionImport` 指令 + lib.txt 函数库解析)。此决策用户已知晓并授权,若用户醒来要求保留规则文件可再议。

6. **全自主运行模式**:用户睡觉期间全自主运行(权限已设为最高)。用户不在线时不发问、不卡住;遇到无法自行决定的问题,记录到 PROJECT_NOTES.md,继续推进能推进的部分。

7. **给用户的可见产物**:所有重要决定、进度、问题、恢复方法都要更新到 `PROJECT_NOTES.md`(用户醒来读这个)。

## 项目架构(目标)

```
hlsl_blend_dxbc_translator/
  src/           C++ 源码(swizzle、types、ast、lexer、parser、symbols、intrinsics、codegen、translator、main)
  data/          functions/lib.txt(函数库)
  tests/         从 Python 项目复制的测试样例
  .vcxproj       构建(VS2022 v143)
```

## 参考文件位置

- DXC:`E:\Project\Open\DirectXShaderCompiler`(HLSL 前端在 tools/clang/lib/ 的 Parse/Sema/AST,DXIL 相关在 lib/HLSL/)
- vkd3d:`E:\Project\Python\hlsl_blend_dxbc_translator\参考\vkd3d-1.19`(hlsl 编译器在 libs/vkd3d-shader/hlsl.c / hlsl_codegen.c 等)
- Python 原版:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\`
- 测试样例:`E:\Project\Python\hlsl_blend_dxbc_translator\PythonMain\Test\`
