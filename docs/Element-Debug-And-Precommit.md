# 元素调试与提交格式门禁

## pre-commit

本地 core.hooksPath 已设置 .githooks；跨电脑克隆后运行 git config core.hooksPath .githooks。
版本化文件：.githooks/pre-commit、Tools/check_cpp_format.py、Source/.clang-format。
只读检查暂存区Source下.h/.cpp，不自动改文件、不自动暂存。部分暂存时读git show :path，忽略工作区尚未暂存内容。
只提交资产时不运行C++格式检查。需要Python和clang-format22+；设置CLANG_FORMAT可覆盖工具路径。
第一次提交须把Source/.clang-format同时加入暂存区。当前暂存区仍是格式化前的内容，提交前要审查后重新git add相应源码；不能因为工作区已格式化就认为暂存区也已更新。
独立临时仓库测试通过：格式正确放行、部分暂存不误读工作区、格式错误阻止、不修改index或工作文件。本仓库未提交/未改暂存区。

## 调试开关

默认关闭，仅Development/Editor等非Shipping可用。PIE游戏获得焦点后按~控制台输入：

```text
Loong.Elements.Debug 1
Loong.Elements.Status
Loong.Elements.TestMode 1
```

Debug：0.2秒更新屏幕摘要及世界文字；默认最多12个敌人，优先正在生效元素。
Status：主动执行时打印一次摘要到输出日志并在屏幕保留8秒，平时没有逐帧日志。
TestMode：仅本次进程的验证配置，保证已装备、有效元素模块的概率触发并绕过技能触发冷却；普通直接命中降低至最多1伤害，关闭旧直接额外元素伤害，给持续效果足够观察时间。
不创建不存在的龙、不修补无效技能、不绕过状态施加冷却、不改DataAsset数值、不影响基地逻辑。
关闭：Loong.Elements.TestMode 0；Loong.Elements.Debug 0。控制台变量可跨关卡保留，回正式平衡测试前务必关闭。
可调：Loong.Elements.DrawInterval 0.5；Loong.Elements.MaxTargets 6。

## 观察

HUD显示实际Definition路径、EquipPresets、装备数量、各DragonId/模块数量、每技能配置概率/剩余冷却/最近原因、英雄命中和成功施加次数。
FIRE：橙色文字，层数、剩余时间、周期伤害、实际最近跳伤及tick次数。
ICE：青色，slow百分比、frozen、剩余时间、最后一次实际移动速度。目标停止时实际速度可为0；升级暂停状态参考HUD state。
WIND：绿色圈为范围，标记本次命中数、总实际周期伤害、位移长度和最多6条位移线。显示由低频Director绘制统一调度，伤害循环不高频绘制。
剩余时间升级暂停不变，恢复后继续推进；组件仍由Enemy Tick驱动，无单独每帧全图搜索。
世界文字不等于正式特效，未制作Niagara火焰/冰壳/旋风。

## 已确认链路与原观察困难

实际保存的L_Combat Director引用DA_Combat_Phase6_Test；该资产EquipPresets=true，三个预设各1个模块。GameMode.CombatDefinition=None，不会覆盖Director配置。
自动化验证三DragonId开始注册，enemy元素组件存在，施加/更新/减速冻结/周期伤害/暂停恢复都工作。
原实现没有默认可见状态文字，视觉依赖可选Niagara资源；此外只有英雄命中后存活目标才施加，原配置概率30/45/30%、风冷却2秒和低敌人生命导致效果观察窗口短。
伴战龙弹丸当前不调用英雄元素触发入口。Debug会明确显示无模块、无存活目标、配置无效、概率失败、技能/目标施加冷却。

## 验收步骤

1. 重启UE打开现有L_Combat，保持测试数据指派，无需新蓝图节点。
2. PIE先开Debug，看Definition应为DA_Combat_Phase6_Test，FireDragonTest/IceDragonTest/WindDragonTest列出模块。
3. 开TestMode，靠近近战怪让自动攻击连续命中同一目标；火/冰层数增长，约3次成功冰施加后frozen，风圈出现。
4. 看橙色周期伤害计数、青色实际速度下降、绿色命中目标及位移线。
5. 升级界面出现时状态剩余时间停止，选择升级后继续；关闭TestMode重新观察正式概率/冷却。
6. 若HUD路径或presets不符先检查运行地图Director及GameMode配置，不要只检查正在打开的资产编辑器。

## 限制

本批自动化是NullRHI，不能证明真实DrawDebug在用户PIE窗口的视觉最终效果；仍需以上人工观察。
调试文字英文以方便识别配置字段，正式UI/美术在B线替换。
调试模式不用于平衡或性能结论，默认关闭，Shipping不生效。

## 最终验证结果

Editor/Game真实构建均成功，日志Saved/Logs/LoongElementDebugEditorBuild.log与LoongElementDebugGameBuild.log。
实际Unreal自动化8成功、0失败、0警告、0跳过，退出码0；报告Saved/Automation/ElementDebug/index.html。
只读实际资产审核：测试资产EquipPresets=1、三个预设各1模块，所有Spec有效，三项VisualEffect均为空；火/冰/风概率为0.30/0.45/0.30，冷却0.25/0.25/2.00，持续3秒。
实际地图Director引用该测试资产，GameMode无覆盖定义。未修改/保存任何现有uasset/umap。

pre-commit临时仓库测试全通过；本地实际git hook run已经调用门禁，因Source/.clang-format尚未暂存而正确阻止。首次提交要包含规则文件，随后只检查暂存代码；不会自动暂存。
跨Linux克隆要确保pre-commit文件可执行：git update-index --chmod=+x .githooks/pre-commit（文件已暂存后执行）。
当前本地Windows hook运行正常。未提交Git，未改变用户暂存区。

## 纯UI验收入口（2026-10-13更新，优先使用）

进入L_Combat的PIE后，右上角“元素测试（展开/收起）”即可操作：
状态可视化按钮、高触发测试按钮、结束测试恢复正常按钮。摘要在面板内显示并可滚动，无需控制台输入。
默认收起、开关默认关闭，仅非Shipping测试构建显示。Widget离开战斗/结算时恢复进入测试前设置。
高触发模式用于验收，不修改正式数据资产或永久平衡参数。
Editor/Game构建成功；实际回归8项成功、0失败、0警告。NullRHI未验证按钮视觉/真实鼠标操作，需PIE人工验收。
以前的控制台接口仅留作开发内部工具，玩家/日常验收不要求使用。
