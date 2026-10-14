# P0-01A 开发报告：火、冰、风元素稳定性

日期：2026-10-14。结论：C++ 修复完成；Editor / Game 构建成功；12 项自动化通过。画面、真实输入与跨地图返回仍需要用户在 UE5 PIE 中验证。

## 1. 本次完成的工作

- 接通伴战龙弹丸 → ApplyHit → 自身 ElementAbilities → 状态组件的链路。伴战龙只使用自己的模块，英雄继续使用已装备的被动模块；来源、触发冷却、卸下清理规则保持一致。
- 修复负向风位移超过距离时穿过牵引点的抖动，牵引抵达中心后停止；正向击退、范围跳伤与场地边界限制保留。
- 缺少 Niagara 或加载/创建失败时，默认创建可见占位反馈：火为橙红色上升火点，冰为青色冰锥（冻结时变大），风为绿色范围轨道标记。每个效果仅一个 InstancedStaticMeshComponent、四个实例，不新增效果 Actor、不使用全图每帧搜索。
- 新增 bUseFallbackVisual、VisualOffset、VisualScale 配置。可关闭占位反馈；缩放独立于目标根组件，默认偏移为 (0,0,30)cm。
- Niagara 创建时先设置参数再激活；升级选择暂停 Niagara，选择后恢复。占位动画使用战斗时间，暂停不推进。死亡、过期、卸下与结算清理视觉组件。
- 参数校验增加元素枚举与视觉变换检查，元素入口补充目标/组件有效性检查。状态调试文字增加实际视觉路径/回退原因，平时不增加高频日志。
- 新增四项回归与可执行 PowerShell 验证入口，未改存档格式、已有反射类/函数名称或资产文件。

## 2. 发现的问题

### 实际项目检查

- 项目：`E:/ue5_Project/Loong/Loong.uproject`，EngineAssociation=5.8。
- 实际引擎：`D:/soft/epic/UE_5.8`，Build.version=5.8.3，CL=58210709。
- 已读最新 `docs/Loong_GDD_v1.0.md`；未找到 `Loong_P0_Development_Plan.md`，按本任务与 GDD 开发。
- 初始 Git 状态只有用户的 `docs/Loong_GDD_v1.0.md` 未跟踪；未覆盖该文件，未改暂存区、提交或推送。
- `.gitignore` 已排除 Binaries、Intermediate、Saved、DerivedDataCache，无需修改。没有项目级 Plugins 目录；uproject 显式启用 ModelingToolsEditorMode、GameplayStateTree，模块依赖 Niagara。
- 角色默认输入类为 EnhancedPlayerInput / EnhancedInputComponent，已有 IMC_Default、IA_Move 等输入资产；C++ 战斗使用自定义 AutoAttack / DragonAbility / ElementComponent，没有 GAS 模块依赖或 GameplayAbility 实现。
- 真实地图 `/Game/Loong/Maps/L_Combat` 的 `LoongCombatDirector_1.Definition` 指向 `/Game/Loong/Data/Combat/DA_Combat_Phase6_Test`。地图 GameMode 无 CombatDefinition 覆盖。
- 真实资产 Equip Element Test Presets=true，三个预设各一个有效模块。StartCombat 确实遍历并装备它们，敌人 Configure 确实将元素组件连接到同一 Director。

### 根因及源码位置（本次修改后的行号）

| 问题 | 证据与修复位置 |
|---|---|
| 伴战龙普通命中不触发元素 | 原 ApplyHit 仅在 bHero=true 分支调用元素入口；`Source/Loong/LoongCombatDemo.cpp:776` 接通伴战命中，`:675` 按 DragonId 过滤来源模块 |
| 未配置 Niagara 时没有普通战斗视觉 | 真实三项 VisualEffect 全为空；原 ApplyEffect 只在软引用非空时创建 Niagara。`Source/Loong/LoongElementComponent.cpp:40` 创建回退组件，`:105` 更新形状/动画 |
| 风牵引越过施法来源点 | 原每次直接移动固定负步长；`Source/Loong/LoongElementComponent.cpp:328` 将牵引长度限制为剩余距离 |
| Niagara 生命周期可能与战斗状态不一致 | 原组件自动激活后才设置参数，也未在升级选择时暂停；`:55` 延迟激活，`:146` 暂停接口，`LoongCombatDemo.cpp:882` / `:909` 接入升级流程 |
| 冻结很难在普通测试数值下观察 | 真实 HeroStats.AttackDamage=25，近战 HP=45；龙加成后攻击更高。普通目标常在三次成功冰施加之前死亡。火/冰/风概率为 30%/45%/30%，风冷却 2 秒，持续时间均为 3 秒。保留正式数值，通过现有高触发测试入口进行观察 |

修复前实际运行新增的前三项测试，三项均失败（伴战元素、视觉缺失、风过冲），报告：`Saved/Automation/P0-01A-Before/index.json`。不是仅依据推测修改。

## 3. 修改文件

| 文件 | 用途 |
|---|---|
| `Source/Loong/LoongCombatDemo.cpp` | 伴战元素来源路由、升级视觉暂停/恢复、有效性检查、配置诊断 |
| `Source/Loong/LoongCombatTypes.h` | 三个可兼容追加的视觉配置字段 |
| `Source/Loong/LoongElementComponent.h` | 视觉组件引用、占位资源与生命周期接口 |
| `Source/Loong/LoongElementComponent.cpp` | 占位表现、Niagara 激活/参数、牵引修复、视觉清理 |
| `Source/Loong/Tests/LoongElementStabilityTests.cpp` | 伴战来源/冷却、风位移、视觉生命周期、实际保存地图配置验证 |
| `Tools/Test-P0-01A.ps1` | 检查引擎版本、编译 Editor、运行全套 Loong 测试并严格检查新生成 JSON |
| `docs/P0-01A-Development-Report.md` | 本报告与人工验收步骤 |

没有修改任何 `.uasset` / `.umap`、Config、存档实现或其他任务文件。C++ 仅对新增/修改区域及新测试文件格式化，`git diff --check` 通过。

## 4. 编译结果

实际工具链：Visual Studio Community 18，MSVC 14.51.36260，Windows SDK 10.0.26100.0，引擎自带 .NET 10。

在项目根目录实际执行：

```powershell
& 'D:/soft/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' LoongEditor Win64 Development '-Project=E:/ue5_Project/Loong/Loong.uproject' -WaitMutex -NoHotReloadFromIDE '-Log=E:/ue5_Project/Loong/Saved/Logs/P0-01A-EditorBuild.log'
& 'D:/soft/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' Loong Win64 Development '-Project=E:/ue5_Project/Loong/Loong.uproject' -WaitMutex -NoHotReloadFromIDE '-Log=E:/ue5_Project/Loong/Saved/Logs/P0-01A-GameBuild.log'
```

两项最终均 Result: Succeeded、退出码 0。一次中途编译发现新测试将 TArray<TObjectPtr<AActor>> 写为 auto* 遍历，产生 C3535/C2440；已改为 const auto&，重新真实编译通过。失败记录保留在 `Saved/Logs/P0-01A-EditorBuild-FailedAttempt.log`。

编译不是零警告：存在引擎头文件 C4996（Niagara/UObject 资产标签、APawn 接口弃用）、MSVC 非首选版本与旧 IncludeOrder 提示。未为消除这些已有环境提示修改引擎或扩大项目升级范围。

## 5. 自动测试结果

最终实际执行 `./Tools/Test-P0-01A.ps1 -SkipBuild`，退出码 0，输出 `PASS: 12 tests`。不带 SkipBuild 可重新编译再运行。

底层命令：

```powershell
& 'D:/soft/epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/ue5_Project/Loong/Loong.uproject' -unattended -nop4 -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests Loong.' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/ue5_Project/Loong/Saved/Automation/P0-01A' '-AbsLog=E:/ue5_Project/Loong/Saved/Logs/P0-01A-Tests.log'
```

报告：`Saved/Automation/P0-01A/index.json` / `index.html`。12 成功、0 测试警告、0 失败、0 跳过。

| 测试 | 已验证内容 |
|---|---|
| P001A.CompanionElements | 实际弹丸命中只附加伴战自身燃烧；不借用其他龙冰能力；重复 ResolveHit 不重复施加；冷却与卸下清理 |
| P001A.WindDisplacement | 牵引停在来源点、中心稳定、范围内伤害、范围外排除、正向击退 |
| P001A.ElementVisibility | 三元素无 Niagara 时创建注册/可见/无碰撞实例组件；暂停动画不推进；恢复后过期清理；关闭回退；无效参数；跳伤致死时清理 |
| P001A.SavedCombatConfiguration | 只读加载真实地图并解析 Director/GameMode 的有效 Definition；复制到临时世界验证三元素施加、冻结、周期伤害和风命中。仅提高临时副本敌人血量，不改保存资产 |
| Phase6.ElementDebugChain / EnemiesAndElements | 预设实际装备、燃烧层数/跳伤、冰减速/冻结恢复、效果并存、暂停/恢复、远程弹丸生成/伤害、结算清理 |
| LoopAndAbilities | 自动攻击、龙加成、敌人追击/伤害、生成上限、升级与溢出、完整 180 秒模拟、胜负一次性结算、基地系统禁用 |
| Building.Grid 三项 / Interaction.Lifecycle / Persistence.RoundTripAndFailures | 网格/旋转、防重叠、建造交互、保存读取与失败保护回归 |

首轮完整回归曾因新测试调用 SetByConsole 干扰旧测试的 SetByCode 优先级而失败；已将测试变量修改/还原改为 SetWithCurrentPriority，最终整套通过。生产 UI 开关没有改变。

验证脚本检查 JSON 的失败/警告/未运行数量和生成时间：本机 UE 在测试断言失败时也可能因 TestExit 返回 0，不能只看进程退出码。

`BaseSave_01.sav`、`BaseSave_01_Backup.sav`、`BaseSave_01_Integrity.sav` 在测试前后 SHA-256 全部相同，记录位于 `Saved/P0-01A-SaveHashes-Before.json` / `After.json`。存档测试使用原有随机独立测试槽。

未验证：GPU 渲染、实际鼠标按钮、真实蓝图玩家输入、正式 Niagara 的视觉/暂停、复杂场景位移碰撞、PIE 跨关卡返回。NullRHI 注册可见组件不等于屏幕视觉已验收。

## 6. UE5 PIE 人工验收步骤

以下全部标记为 **需要用户在 UE5 PIE 中验证**。

1. 关闭并重新打开 UE5 项目以加载新 DLL 与追加字段；在“内容浏览器”（Content Browser）打开 `Content/Loong/Maps/L_Combat`。
2. 在“大纲视图”（World Outliner）选择 `LoongCombatDirector_1`，在“细节”（Details）检查 Definition=`DA_Combat_Phase6_Test`。在“世界设置”（World Settings）检查游戏模式覆盖为 LoongCombatGameMode；GameMode 的 Combat Definition 当前为 None。
3. 双击该数据资产，确认 `Equip Element Test Presets` 勾选。展开 `Element Dragon Presets`，确认 FireDragonTest / IceDragonTest / WindDragonTest 各自 Element Abilities 非空。此保存资产已正确配置，正常无需改动。Visual Effect 为空时，Use Fallback Visual 默认开启即可显示占位表现。
4. 点击工具栏“运行”（Play）。展开右上角“元素测试（展开 / 收起）”，点击“开启状态可视化”和“开启高触发测试”。摘要应显示正确 Definition、presets=1，以及三个 DragonId。靠近敌人，英雄与伴战龙自动攻击，不需要新增按键绑定。
5. 火：观察目标附近橙红色上升火点；FIRE 显示层数/剩余时间，actual last tick 和 ticks 更新，敌人出现持续跳伤。
6. 冰：对存活目标连续施加，观察青色冰锥、slow 增长和移动减慢；达到阈值出现 frozen=1、冰锥变大并停止自身移动。停止追加攻击后观察冻结结束、减速持续，最终效果消失并恢复移速。
7. 风：观察绿色轨道标记/调试圈、area hits 与 moved 数值，以及周围敌人受范围伤害和向最近施加来源位置牵引。伴战风龙命中现在也能施加风。接近牵引中心后不应反复越过中心跳动。
8. 升级三选一出现时，观察计时、状态剩余时间、攻击与占位动画停止；选择一项后恢复。点击“结束测试：恢复正常”，确认正常直接伤害/概率恢复，再检查一次升级流程。
9. 正常阶段推进至约 60 秒后，观察橙色快速怪和紫色远程怪。移动避开远程直线弹丸，再主动承受一枚检查伤害；升级暂停时弹丸应暂停。约 120 秒检查生成压力与数量上限。
10. 完成胜利及另一次失败测试，确认结果界面只结算一次，场上敌人/弹丸/元素组件清理；点击“返回基地”，确认到 L_Base。检查原建筑、200cm 放置预览/防重叠，执行一次有意的基地保存并退出重进验证读取。不要把“原存档哈希未变”当作这一步已自动通过。

### 可选：关联正式 Niagara

不关联正式资源也可验收本次占位表现。需要正式素材时，在数据资产的 `Element Dragon Presets → 对应预设 → Element Abilities → Effect → Visual Effect` 指派实际 Niagara System（不是 Emitter）。

在 Niagara 编辑器“参数”（Parameters）中声明并使用：`User.Duration`（Float）、`User.Radius`（Float，厘米）、`User.Stacks`（Int）、可选 `User.Frozen`（Bool）。状态效果资源应使用适合持续附着的循环与固定边界，避免一次性发射后提前不可见；组件随状态销毁。通过 Visual Offset / Visual Scale 调整位置与尺寸；绑定成功后代码优先使用 Niagara，不再叠加占位组件。保存通过编辑器执行，代码未操作资产二进制。

## 7. 遗留问题

- 真实画面、可读性、按钮操作与跨关卡返回尚未进行人工 PIE 验收；不能标记视觉最终通过。
- 三项正式 Niagara 资源仍未关联；当前交付为可关闭的简易占位反馈，没有声称完成正式火焰/冰壳/旋风美术。
- 原测试数值下冰冻观察窗口短，需使用现有高触发模式进行机制验收；正常数值下的触发率/敌人血量平衡仍需试玩。
- 原风位移仍使用 SetActorLocation 与场地边界限制，未增加复杂墙体扫掠/导航；软 Niagara 首次加载仍为同步加载。

## 8. 下一开发任务

C++ 编译和自动回归门槛已具备。先完成上述 PIE 验收，再进入后续 P0-01B 或 P0-02A；当前不将未执行的画面与跨地图验证标为通过。

缺少 Loong_P0_Development_Plan.md，无法据此确定这两个编号的详细范围。按 GDD 的顺序，下一阶段应在战斗验收通过后推进人龙协同原型；本任务没有提前开发龙种、繁殖、骑乘或地图。

### 后续 PIE 反馈：WASD 方向修正（2026-10-14）

用户反馈 A 上、D 下、W 右、S 左。只读导出实际 BP_TopDownCharacter 后确认：IA_Move 读取角色自身 Camera 的世界旋转，再将 Yaw 转为 Forward/Right 移动方向；战斗的独立 LoongArtTestCamera 接管画面后，两套 Camera 的 Yaw 不一致。

修改 `LoongCombatArtTest.h/.cpp`：独立战斗相机更新视图时，同步角色 Camera 的世界旋转，并使用绝对旋转避免角色转身再次改变移动参考。没有修改输入映射或蓝图资产。新增 `Tests/LoongMovementCameraTests.cpp`，使用实际角色蓝图验证多个相机朝向、转身与运行时朝向变化。

整套实际自动化：13 成功、0 失败、0 测试警告、0 跳过；报告 `Saved/Automation/MovementCamera/index.json`。NullRHI 不代替用户实际按键验收。

初次标准 Editor 链接因正在运行的编辑器占用 DLL 而失败（LNK1104），随后独立后缀模块链接成功并完成测试。编辑器退出后再次执行标准构建，成功生成 `UnrealEditor-Loong.dll`：

```powershell
& 'D:/soft/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' LoongEditor Win64 Development '-Project=E:/ue5_Project/Loong/Loong.uproject' -WaitMutex -NoHotReloadFromIDE '-Log=E:/ue5_Project/Loong/Saved/Logs/MovementCamera-EditorBuild-Final.log'
```

本轮开始时用户的 L_Combat.umap 已有修改，已保留；未编辑该地图或其他二进制资产。重新打开编辑器进入 L_Combat 的 PIE，验证 W 上、S 下、A 左、D 右，以及角色转身后方向不变。

### 后续截图反馈：元素调试显示可读性（2026-10-14）

用户确认移动恢复正常。两张截图显示三元素状态已施加、冰减速和风周期伤害有反馈，但场景多行文字严重重叠，左上配置摘要重复覆盖 HUD，右侧按钮文字超出面板。截图的 frozen=0、部分 moved=0 不足以判定冻结/牵引已完成 PIE 验收。

本轮修改：

- `LoongElementComponent.cpp`：原 DrawDebugString 调用在 UE 内部使用 bSkipOverwriteCheck=true，重复追加条目。改为 HUD.AddDebugText 按目标 Actor 替换一个附着短标签；保留风圈/位移线，不在空状态敌人上绘制大段文字。
- `LoongElementDebug.cpp`：默认 MaxTargets 从 12 改为 1；`LoongCombatDemo.cpp` 优先显示玩家附近正在生效的目标，不再循环输出屏幕左上摘要。无元素的普通伴战龙现在明确报告 normal attack only，避免错误暗示整个已装备模块缺失。
- `LoongCombatWidget.h/.cpp`：按钮字体 14、提示字体 12，允许换行；面板宽度在 260–400 范围随视口调整，滚动区域高度随视口调整；当前目标状态放在面板前部，详细配置随后显示。

只读实际资产检查：冰 slow_per_stack=0.15、freeze_at_stacks=3、freeze_duration=0.6；风 radius=250cm、displacement_per_tick=-25cm、tick_interval=0.5。未修改这些资产数值或任何 uasset/umap。

Editor 使用 `-ModuleWithSuffix=Loong,1015` 编译和链接成功，日志 `Saved/Logs/ElementReadability-EditorBuild-Final.log`。期间修正了 TArray<TObjectPtr> 排序比较器的自动解引用，以及 UButton 子槽 Padding 的 API 使用错误。当前打开的编辑器仍使用旧代码，需要保存并重启后加载新模块。

实际全套 Loong 自动化：13 成功、0 失败、0 测试警告、0 跳过，报告 `Saved/Automation/ElementReadability/index.json`。NullRHI 未验证新布局最终画面。重启 PIE 后检查按钮无裁切、仅一个附近目标短标签；在右侧滚动面板观察燃烧跳伤、frozen=1/恢复，以及存活目标的风位移。没有依据静态截图将这些剩余人工项标为通过。
