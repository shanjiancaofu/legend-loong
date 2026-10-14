# Loong 自动化测试（P0-QA-01）

使用 UE 原生 Automation Test Framework。测试位于现有 `Source/Loong/Tests`，编译受 `WITH_DEV_AUTOMATION_TESTS` 控制；没有新增运行时依赖、测试模块、地图或二进制资产。

UE 5.8.3 安装中有 `Engine/Source/Developer/CQTest`（及测试插件），它扩展原生框架提供 fixture/异步命令。当前用例都是确定性计算或同步临时世界行为，现有框架加 RAII 已足够；本轮不为改写已通过的测试引入 CQTest/FunctionalTest。未来真实 PIE 流程可再考虑引擎 FunctionalTest/CQTest。

## 常用命令

在项目根目录使用已安装的 PowerShell 7（`pwsh`，本次实际为 7.6.7）。脚本要求 7.0 及以上：

```powershell
# 默认先编译 Editor，再运行选定测试
./Tools/Run-LoongTests.ps1 -Group Quick -ChangedModules Combat,Building
./Tools/Run-LoongTests.ps1 -Group Elements -ChangedModules Elements
./Tools/Run-LoongTests.ps1 -Group Building -ChangedModules Building
./Tools/Run-LoongTests.ps1 -Group Persistence -ChangedModules Persistence
./Tools/Run-LoongTests.ps1 -Group Full -ChangedModules Tests,Combat,Building,Persistence

# 同一份 C++ 已成功编译时才可跳过编译
./Tools/Run-LoongTests.ps1 -Group Combat -SkipBuild

# 自定义名称/前缀，用 + 合并；每项必须以 Loong. 开头
./Tools/Run-LoongTests.ps1 -TestFilter 'Loong.Combat.Status.+Loong.Building.Grid.' -SkipBuild

# 自定义报告父目录；每次创建独立时间戳+GUID子目录，避免旧报告误判
./Tools/Run-LoongTests.ps1 -Group Full -ReportPath Saved/Automation/QA

# 运行器本身的报告判定验证
./Tools/Testing/Test-AutomationReport.ps1
./Tools/Testing/Test-Process.ps1

pre-commit run -a
```

`Tools/Test-P0-01A.ps1` 保留为兼容入口，现在调用同一个 Full 运行器。

引擎解析顺序：`-EngineRoot`、`UE_ENGINE_ROOT`，否则检查 EngineAssociation 对应的 Windows 注册表、Epic Launcher 安装清单和正在运行的 Editor 路径。候选必须有 Build.version、匹配关联版本并包含 UnrealEditor-Cmd。Editor Target 从实际 Source/*Editor.Target.cs 发现；不明确时使用 `-EditorTarget` 指定。脚本没有默认硬编码引擎安装路径。

编译直接调用解析到的引擎自带 `dotnet.exe` 与 `UnrealBuildTool.dll`。发现该项目 Editor 正在运行时，为项目模块生成独立 DLL 后缀，避免覆盖已加载 DLL；不会关闭现有编辑器。需要重启 Editor 才能在人工 PIE 中使用新代码。Editor 关闭时执行标准构建。

## 分组与覆盖

组定义在 `Tools/Testing/TestGroups.json`；UE 的名称前缀过滤采用 `^前缀+^前缀`。

| 组 | 本轮源码预期数量 | 覆盖 |
|---|---:|---|
| Quick | 10 | 伤害/阶段纯计算、三元素状态时间与位移、网格计算；不读取玩家槽或真实项目地图 |
| Combat | 14 | 全部战斗测试，包括旧 180 秒逻辑模拟和真实保存配置审核 |
| Elements | 9 | 状态时间、元素/敌人、伴战来源、视觉组件生命周期、保存配置审核 |
| Grid | 5 | 200cm 吸附、正负坐标、半格、占地、旋转与基地边界 |
| Building | 8 | 网格、放置交互及建筑存档 |
| Persistence | 2 | 原存档失败保护/恢复和新增扩展状态/清理 |
| Camera | 1 | 相机方向参考（不代替手感验收） |
| Full | 22 | 全部 Loong 原生自动化 |

预期发现名单从项目源码中的 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 名称提取并按组筛选，不用固定的“至少 12 项”判断。新增这类测试会自动加入前缀组。若未来采用 Complex Test、Spec、CQTest 的动态子用例，需扩展发现清单逻辑；当前运行器仍接受额外的引擎报告结果，但不能只靠源码正则保证那些动态用例均被发现。

快速测试仍需启动 UE，因此包含编辑器启动成本。180 秒旧战斗用例是推进逻辑时间，非固定 Sleep 180 秒。

## 新增的九项真实测试

| 完整名称（前缀 Loong.） | 行为与依赖 |
|---|---|
| Combat.Calculation.HealthDamageGuards | 真实 HealthComponent：正常/无效/NaN/无限伤害、过量伤害截断、死亡回调一次；不需要世界 |
| Combat.Calculation.SpawnPhaseBoundaries | 真实 Definition：0/60/120 边界、无序阶段配置、固定种子、空阶段回退；不需要世界 |
| Combat.Status.FireTickPartitionAndExpiry | 小步/大步相同总跳伤、首次跳伤前无伤害、最后有效跳伤与到期停止；临时世界/Actor |
| Combat.Status.IceFreezeRecovery | 实际敌人速度、三次施加冻结、升级不消耗冻结时间、恢复后解除冻结与减速；临时世界/Actor |
| Combat.Status.WindRadiusBoundary | 精确范围边界有效、范围外无伤害/位移、牵引方向和保持 Z；临时世界/Actor |
| Combat.Lifecycle.SettlementReentry | 胜/负奖励快照、反向重复结束/死亡/经验/时间入口不重结算；临时 Director |
| Building.Grid.SnapSignedCoordinates | 正负位置、±半格、幂等与保留 Z；无美术/Actor |
| Building.Grid.EdgeAndQuarterTurns | 负/大旋转序号、无效占地、偏移基地的贴边/越界、旋转占地；无美术/Actor |
| Building.Persistence.ExtensionStateAndCleanup | 真实磁盘存取、GUID/旋转/负坐标/Unicode 扩展值、重复载入无重复实体、槽清理；临时世界/Actor |

原有 13 项保留。旧 ElementDebugChain 改用 SetWithCurrentPriority 保存/还原 CVar 值，避免 UI 或其他测试提升优先级后导致开关修改被忽略。新 fixture 在析构时回收世界、世界上下文、战斗 Actor 和进程级测试模式。

当前没有持久奖励钱包/完整游戏进度存档字段。结算测试验证现有 SettlementCount 与 Result.TestReward 的不可重复变化，不能声称已经验证未来钱包支付。存档测试的 `QA_Progress` 是现有建筑 State.Values 的测试扩展载荷，不能声称完整冒险/龙族进度保存已实现。

## 判定、报告与安全

- 每次运行创建独立目录，包含 Build.log、Unreal.log、进程输出、引擎 index.json/index.html 和运行器 summary.json。
- summary 记录引擎、目标、测试组、源码发现名单、声明修改模块、Source/Config 改动、构建是否跳过、后缀、结果及存档检查。
- 成功必须满足：构建成功（除非明确 SkipBuild）、进程成功、报告新生成、非空发现、所有预期名称出现、没有重复/失败/跳过/进行中/测试警告，全部状态 Success。
- UE 的 `-TestExit=Automation Test Queue Empty` 可能在断言失败时返回 0，因此 JSON 是主要判据。编译器环境警告与测试警告分别看待。
- 构建默认超时 1800 秒、测试默认超时 300 秒，可用 BuildTimeoutSeconds/TestTimeoutSeconds 修改。等待进程退出条件，每秒检查超时，每 30 秒输出进展；超时只结束本运行器启动的进程树。
- 使用文件句柄锁防止两个运行器同时改写模块元数据；不会通过删除锁文件强行并发。
- 每次前后对现有 Saved/SaveGames/*.sav 做 SHA-256 比较，并检查新产生的测试槽残留。若改变，只报告失败，不自动覆盖/恢复玩家文件。运行时避免同时在另一个 PIE 会话保存基地，否则无法区分外部保存与测试改写。
- 新磁盘测试使用 Loong_QA_<GUID>；原测试使用 Loong_Automation_<GUID>。仅回收自身唯一槽的主文件/Backup/Integrity，正常退出清理。进程崩溃时析构无法保证执行，残留文件会使运行器失败；不自动删除不明旧槽。
- 默认产物在被 .gitignore 排除的 Saved 下。源码、脚本、组配置、说明可提交；报告/日志/构建产物不提交。

## Codex 每次修改的流程

根目录 AGENTS.md 已加入同一流程：记录模块 → 改代码 → 编译 Editor → 按组测试 → 读取实际错误/日志并修复 → 重跑受影响检查 → 报告结果与仍需 PIE 验证内容。公共 fixture、运行器或多个模块变动使用 Full。

失败时先查看运行器打印的 summary.json，再看 Build.log 或 Unreal.log，以及 index.json 中失败用例 entries。不要重试到偶然成功、删除关键校验或把未运行测试写成通过。

## 手动 PIE 的边界

NullRHI 测试不证明 Niagara 渲染质量、按钮最终布局、鼠标真实操作、镜头手感、骑乘手感、GPU 性能或真实地图返回。仍需在 L_Combat 检查技能反馈、暂停/恢复/结算与返回 L_Base，在基地有意保存并退出重进。测试读取保存地图/资产仅作配置审核，不编辑或保存 uasset/umap。

## P0-QA-01 实际验证记录

本轮工作区开始时干净，HEAD=`eefeef3`。实际项目关联版本 5.8，自动发现引擎 `D:/soft/epic/UE_5.8`，Build.version=5.8.3 / CL 58210709。目标为 LoongEditor Win64 Development，使用引擎自带 .NET 10、MSVC 14.51.36260、Windows SDK 10.0.26100.0；运行器实际执行命令和退出码在对应目录日志中。

| 实际运行 | 结果 | Saved/Automation/P0-QA-01 下的目录 |
|---|---|---|
| 初次 Quick 构建 | 失败：新 fixture 访问私有 bEndingPlay；移除该访问，未改变生产权限 | Quick_20261014_195926_c76da77d |
| 修复后 Quick | 构建成功；9 通过、1 失败，进程返回 0 但脚本正确拒绝 | Quick_20261014_200304_7f1cbae1 |
| 最终 Quick | 标准 Editor 构建成功；10 通过、0 失败/警告/跳过 | Quick_20261014_200643_df8c314d |
| Full | 22 通过、0 失败/警告/跳过；沿用刚成功的构建 | Full_20261014_200927_62a991fa |
| PowerShell 7.6.7 最终 Full | 22 通过、0 失败/警告/跳过；summary 记录实际 PS 版本 | PS7/Full_20261014_201832_a6f4fcae |
| PowerShell 7 自定义联合筛选、带空格报告路径 | 两项指定用例均通过，验证实际入口与参数传递 | Custom Reports/Custom_20261014_202531_f64d5bef |

Quick 唯一断言失败为新冰测试使用 FMath::IsNearlyEqual 的 UE_SMALL_NUMBER 默认容差比较三层 20% 减速产生的 float。改成 TestEqual(..., KINDA_SMALL_NUMBER) 明确要求接近 0.4，并保留实际速度、冻结与恢复断言。不是修改生产状态规则或放宽关键校验来让测试通过。

所有 9 项新增用例均出现在 Full 的引擎 JSON 名单；完整通过数为旧 13 + 新 9 = 22。三份已有 BaseSave_01 主/Backup/Integrity 文件在每次运行前后哈希一致，新增测试槽无残留。构建存在引擎弃用和非首选编译器提示，不能称为零编译警告。

报告校验自测 8 种场景通过：有效报告、真实失败、缺失用例、跳过、警告、空发现、重复、旧报告。进程自测验证非零退出码保留，以及未满足的条件被 1 秒超时结束并保留输出；没有固定 Sleep 充当异步完成条件。

自定义筛选初次验证发现参数 Group 的 ValidateSet 不允许将其重赋值为 Custom；已使用独立 runGroup 标签并重新实际运行通过。较早的 Windows PowerShell 5.1 Persistence 验证也曾通过，但按用户最新要求，现在正式入口明确要求 PowerShell 7，不继续维护旧版兼容路径。

继续开发优先增加实际 L_Combat → 结算 → L_Base 的关卡流程测试，以及未来持久奖励钱包实现后的支付防重入测试。当前没有自动执行上述真实 PIE、骑乘、Niagara 视觉或 GPU 性能验收。
