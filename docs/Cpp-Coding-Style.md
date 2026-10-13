# Loong C++ 排版规范

项目规则：Source/.clang-format。使用 clang-format 22.1.3 验证。

- 4 空格缩进，不使用制表符；大括号使用 Allman 换行。
- 每条语句独立一行，不将条件、赋值、return 和循环压缩成一整行。
- UPROPERTY/UFUNCTION/UCLASS/USTRUCT/UENUM 与对应声明分行。
- 函数和逻辑组之间保留空行，访问限定符与类成员层级清楚。
- 建议行宽110；不拆改字符串内容，不重排包含顺序。
- 函数参数、运算符和逗号保持统一空格。
- 格式整理与功能修改应尽量单独提交，避免审查时混淆。

PowerShell 格式化单文件示例（本机已安装工具）：

```powershell
& "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe" -i Source/Loong/LoongCombatDemo.cpp
```

本次对35个.h/.cpp执行格式整理，并比对格式化前后的代码词元：声明、运算符、标识符、数值和字符串均保持不变；仅空白及注释排版调整。
未更改用户的Git暂存内容，不改资产或地图。
