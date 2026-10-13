# A线第五阶段：180秒轻量肉鸽战斗 Demo

日期 2026-10-12。当前工作区还包含第四阶段未提交存档源码及B线初始准备，不把它们擅自合并提交。

## 实现文件

LoongCombatTypes.h：人物/敌人/龙能力/升级/结果定义 + ULoongCombatDefinition。
LoongCombatDemo.h/.cpp：生命伤害、自动攻击、敌人、弹丸、伴战龙、Director 和 GameMode。
LoongCombatWidget.h/.cpp：中文生命/经验/倒计时HUD、随机强化、结算和返回基地按钮。
Tests/LoongCombatTests.cpp：World 中算法、交互及180秒加速模拟回归。
Tools/CreateCombatTestMap.py：新地图和默认数据资产创建脚本，拒绝覆盖同名资源，拒绝切换未保存地图。
共享B线初始 LoongCombatArtTest.h/.cpp：独立镜头 -55 Pitch/-45 Yaw/2600宽，低FOV对比，未来资源占位和帧统计支持。
Loong.Build.cs 增加Niagara依赖；建造组件BeginPlay增加地图GameMode隔离，不重写基地算法。

## 架构

Director 是单局战斗流程，不负责基地。注册敌人列表最多默认30，只在自动攻击时遍历，不每帧全图Actor搜索。
自动攻击按各自冷却推进，弹丸定向追踪，命中一次；目标死亡或战斗结束安全清理。
敌人直线追击、接触冷却伤害；是开放平面Demo，不实现复杂NavMesh绕障。
龙以DragonId替换装备：Buff重新计算，避免永久叠加；命中额外伤害概率使用可控随机种子；伴战龙有自己的攻击组件/冷却/范围。
经验击杀奖励，等级阈值15+Level*10；候选按权重不放回抽取最多3个，满足最大等级/前置条件，批次编号防止旧UI回调污染下一轮。
临时强化为单局数据，结束时清空。测试奖励只广播OnSettled一次，不写基地存档或永久经济系统。
状态Preparing/Fighting/UpgradeSelection/Victory/Defeat/Results；胜负终态汇总后显示Results。
升级冻结战斗计时、敌人/弹丸/伴战龙推进，Character Movement禁用；选择后恢复。非全局暂停，不影响UI Tick。
单局默认180秒，刷怪间隔随时间缩短，批次随分钟增加；死亡立即失败；结束清理战斗Actor与组件。

## 隔离

L_Combat 只设置该地图 GameMode Override = LoongCombatGameMode，不修改项目默认GameMode或L_Base。
复用 BP_TopDownCharacter 和 BP_TopDownController 的WASD；地图相机独立接管视图。
建造组件在战斗GameMode标签下BeginPlay前段直接禁用输入/持久化/tick，不生成存档UI，不读写BaseSave_01。
旧BP建造白线继续停用。返回按钮打开 /Game/Loong/Maps/L_Base，不进行奖励落盘。

## 最少UE中文版操作（不使用Python也可）

1. 重启UE。文件→新建关卡→基础关卡，另存 Content/Loong/Maps/L_Combat。不要复制覆盖L_Base。
2. 地面用Cube约60x60米：Scale=(60,60,0.5)，位置Z=-25，顶部Z=0；PlayerStart放(0,0,110)。保留方向光/天光，保证碰撞。
3. 世界设置→游戏模式覆盖，选择 LoongCombatGameMode。不要选择 LoongCombatArtTestGameMode（那个仅视觉测试）。
4. 保存并PIE：GameMode自动生成/使用Director、角色、相机、怪、龙、HUD，无须攻击蓝图连线。
5. 可选：放一个 LoongCombatDirector；创建数据资产 LoongCombatDefinition，指派到Director的Definition。按属性调整180秒、难度、模型、Niagara和强化。
6. 不放 LoongCombatArtTestScene 到玩法地图，它的20个视觉假人没有战斗行为，会造成混淆。

## 可选自动地图脚本

启用 Python Editor Script Plugin 后重启，保存当前地图。工具→执行Python脚本→选择 Tools/CreateCombatTestMap.py。
创建 L_Combat、DA_CombatDemo、地面、边墙、灯光、出生点、指派数据的Director。拒绝覆盖已存在地图/配置。
脚本Python语法检查通过；未在本轮UE编辑器实际执行，所以若API兼容问题优先使用上面6步手工方式，不宣称资产已生成。

## 真实验证

Editor/Game：真实UBT成功（Saved/Logs/LoongCombatEditorBuild.log、LoongCombatGameBuild.log）。
实际UnrealEditor-Cmd -nullrhi 跑Loong：6成功、0失败、0警告、0跳过，包含基地/存档5项回归和战斗1项。
战斗测试覆盖目标最近/冷却/弹丸仅命中一次/死亡/敌人追击与接触冷却/生成上限/龙Buff替换移除/100%触发元素/龙独立攻击/升级暂停/前置/等级上限/溢出/过期按钮/结算唯一/本局清理/基地隔离。
另包含加速180秒推进模拟，不是人工真实三分钟PIE，也不是帧率验证。
报告 Saved/Automation/Combat/index.json/index.html；日志 Saved/Logs/LoongCombatTests.log。
地图脚本只做Python语法检查。玩家默认存档未触碰。
编译中有本地UE引擎/Niagara旧API声明C4996警告及MSVC非首选版本警告；本轮测试结果0警告不等于编译0警告。

## PIE验收

- 开始只有战斗HUD，无建造/存档面板；B/M/R/Delete无基地操作。
- WASD移动，角色自动发射弹丸，红色占位怪追踪；敌人死亡击杀/经验增长。
- 青绿色占位龙跟随，不阻挡角色，独立弹丸。
- 达升级条件显示最多3个不同选项，计时和战斗冻结，可鼠标点击；选择后恢复，快速点击不重复应用同一批。
- 躲避并坚持180秒，显示胜利、击杀、等级、测试奖励；大量碰撞伤害则失败。
- 结算之后攻击/刷怪停止；返回基地恢复建造与存档。
- 检查正交视图/WASD方向体验、宽屏/窄屏卡牌可读性。
- 单独验证手动放置Director配置可编辑；不同DragonId可同时配置；随机种子便于复现。

## 已知限制与A/B对接

本轮未进行人工PIE。玩法节奏、真实玩家输入、UI焦点、三分钟可玩性仍需实机验收。
占位模型是方块/球，不含正式动画、手绘材质、声音；Niagara接口可配，未制作正式旋风。
弹丸为追踪直达，敌人不NavMesh绕障，接触采用距离检测；升级稀有度字段作为数据接口，概率由Weight控制。
技能效果先是额外元素伤害，不实现火燃烧/连锁/GAS；奖励不接永久经济；多龙有配置入口，尚未做间距/碰撞队形。
资源指针可替换：人物/敌人/龙静态或骨骼Mesh、材质、ProjectileMesh/Effect、HitEffect、SkillEffect、UpgradeIcon（UI当前文本样式）等。
动画资源与骨骼匹配需B线制作检查；改Mesh不等于自动适配所有动画。
B线现存准备代码保留，但本轮不继续创建美术地图/主材质/正式资源。以后独立分支推进并只改新资源目录。
A线不声称达到参考图视觉质量；风格验证继续以docs/Loong_Art_Tech_Validation_v1.md为准。

建议分开提交此前存档与本轮战斗，战斗消息：[feature]: add roguelite combat demo。
