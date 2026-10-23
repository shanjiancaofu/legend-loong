# P0-02B / P0-02C：伴战操作、上下龙与骑乘战斗原型

日期：2026-10-23。起始分支 main，HEAD `8442a4a`；工作区干净。
已读取 AGENTS.md、测试说明与 GDD；未提交、推送或修改任何 uasset/umap、输入资产、存档格式及引擎文件。

## 1. 本次实现

### 伴战面板

- 左下新增“伴战与骑乘原型”面板，显示当前实体、并肩/骑乘模式、实际技能槽、普攻/技能冷却与操作反馈。
- 试装/切换按钮从当前 Definition 的 Dragons 和 ElementDragonPresets 生成，去重并复用既有 EquipDragon/RemoveDragon。
- 支持卸下实体、召回、重新装备，替换会清除旧来源弹丸/状态；操作在升级暂停和结算后被拒绝。
- 试装克隆使用 `ActiveCompanion_<原ID>`，不覆盖原始被动火/冰配置、不改数据资产；并将额外人物加成设为 0，避免试装平白叠加重复加成。
- 切换后只保留一个实体伴战。原本开局配置允许多个实体的行为保留；主选会优先有元素模块的龙。
- 面板有滚动按钮区，宽度随视口调整，普攻鼠标位于控制面板上时不触发攻击请求。

### 控制架构和上下龙

- 保留已有 ALoongCombatDragon 的 AActor 父类、资源引用、攻击组件、技能与跟随逻辑。
- 新增 `ALoongMountPawn : ACharacter`，仅骑乘时生成，是不可见的碰撞/移动控制载体，不是第二套可见龙或战斗系统。
- PlayerController 实际 Possess 骑乘载体；原始角色可见地附着，原龙附着到同一载体。原角色 Tick、碰撞、移动和自动攻击暂时关闭，避免双份输入/碰撞/输出。
- F 或面板按钮上/下龙；上龙要求主选实体、存活角色、正确控制器、距离 550cm 内且胶囊空间可用。
- 下龙检查八个附近候选点、地面、胶囊重叠和路径扫掠；无安全位置则保持骑乘并提示。正常下龙恢复同一角色的控制权、原碰撞、缩放、Tick、移动模式和自动攻击，龙恢复跟随。
- 死亡、结算、龙销毁、控制载体销毁、控制器丢失、卸下或切换均清理骑乘；场景 EndPlay 不尝试重新 Possess 正在卸载的世界。
- 这是占位挂接姿态，不包含坐姿/鞍座骨骼、骑乘动画或正式体型适配。

### 骑乘移动与战斗

- WASD 依据当前镜头 Yaw 控制载体的 CharacterMovement，拥有标准胶囊碰撞和重力；原角色不再独立响应移动。
- Shift 冲刺：短时提高水平速度，受 1.5s 冷却约束；使用 CharacterMovement 碰撞，不提供无敌帧。
- 左键按住请求原龙攻击。沿用原射程/冷却/弹丸，优先选择射程内最靠近鼠标投影的有效敌人；仍是目标辅助追踪弹丸，不是自由弹道射击。
- 1/2/3 在并肩时发出指令、骑乘时主动释放当前龙对应的 ElementAbilities。主动施加跳过被动触发概率，但保留技能/目标施加冷却与已有效果逻辑。
- 只提供配置中真实存在的槽。现有火/冰/风预设各一个模块，通常只有 1 有技能；2/3 会提示没有该槽，不捏造技能。
- 骑乘时关闭英雄与主龙的自动输出，龙的手动普攻继续使用自身来源，英雄残留弹丸沿用原规则。
- 敌人追击、远程弹丸命中和生成位置使用当前控制载体位置；伤害始终进入原 PlayerHealth，载体不创建第二条生命。
- 升级暂停停止骑乘移动、输入请求和冷却；恢复后原骑手保持移动禁用，直到下龙。战斗镜头平滑跟随当前被 Possess 的对象。

## 2. 修改文件

| 文件 | 用途 |
|---|---|
| LoongCombatDemo.h/.cpp | 手动攻击开关、目标选择、骑乘/主选接口、生命周期接入与共享目标位置 |
| LoongCompanionControl.cpp | 选择/召回/卸下、上下龙、控制权恢复、主动技能、状态和按键入口 |
| LoongMountPawn.h/.cpp | 临时碰撞/移动 Pawn、WASD/左键/F/Shift/技能按键、冲刺与暂停处理 |
| LoongCombatWidget.h/.cpp | 现有 UI 接入与刷新 |
| LoongCompanionPanel.cpp | 实际可点击伴战面板与按钮绑定 |
| LoongCombatArtTest.h/.cpp | 平滑跟随被控制对象，保留既有移动方向校准 |
| Tests/LoongMountTests.cpp | 九项真实原生 Automation 测试 |
| docs/Testing/README.md | 更新发现数量和运行入口 |
| 本报告 | 结果、限制与验收步骤 |

未新增外部框架、Build.cs 依赖、地图或可见龙种。

## 3. 实际构建与自动化

本机引擎已从旧任务中的 5.8.3 更新为 **5.8.4 / CL 58902052**；uproject 仍关联 5.8。
本次实际运行 PowerShell **7.6.8**、引擎内 .NET 10、MSVC 14.51.36260、Windows SDK 10.0.26100.0。

```powershell
./Tools/Run-LoongTests.ps1 -Group Full -ChangedModules Combat,UI,Camera -ReportPath Saved/Automation/P0-02BC
```

最终标准 LoongEditor Win64 Development 构建：Succeeded / BuildExitCode=0。
最终报告目录：`Saved/Automation/P0-02BC/Full_20261023_155712_3bce20a6`。
已读取 summary.json、index.json 和 Build.log：37 成功、0 失败、0 用例警告、0 跳过；TestExitCode=0。
原有 28 项加新增 9 项均被引擎发现；旧元素、伴战、建造/网格、存档、升级/结算回归通过。
SaveHashesUnchanged=true；原有 BaseSave_01 主/Backup/Integrity 均保留，测试槽无残留。

额外实际执行 Loong Win64 Development 的 Game Target 构建，同样 Succeeded、退出码0；
日志 `Saved/Logs/P0-02BC-GameBuild.log`。Editor 和 Game 均已生成当前代码的标准二进制。
`pre-commit run -a` 及新增源码单独格式检查通过，暂存区未改动。

| 新测试（前缀 Loong.Combat.Mount.） | 实测覆盖 | 结果 |
|---|---|---|
| SelectionAndRecall | 试装/切换、未知 ID 不破坏状态、召回、卸下/重装、保留被动、暂停拒绝 | Success |
| PossessionRoundTrip | 三次真实 Possess 往返、骑手/龙附着、原角色碰撞/输入/自动攻击恢复、远距拒绝 | Success |
| ManualCombatAndSharedHealth | 骑乘不自动射击、手动实际弹丸伤害、技能来源/冷却、共享生命无第二健康组件 | Success |
| MovementDashAndPause | 移动请求、冲刺速度和冷却、暂停/恢复、骑手不恢复移动 | Success |
| InterruptionCleanup | 卸下、龙销毁、载体销毁、死亡、胜利、骑乘中切换和控制器丢失 | Success |
| BlockedDismountAndCollision | 实际胶囊扫掠受墙阻挡、包围时拒绝下龙、移除阻挡后正常退出 | Success |
| PanelButtonWorkflow | 真实 WidgetTree 与按钮委托，试装→召回→上龙→下龙→暂停拒绝→卸下→结算拒绝 | Success |
| BlueprintRiderAndLevelEndPlay | 实际 BP_TopDownCharacter 上下龙、再次骑乘后 World.EndPlay(LevelTransition) 清理 | Success |
| KeyboardInputRouting | 真实 PlayerInput 输入栈发送 W/释放/Shift/F，验证移动请求、冲刺速度、下龙和控制权恢复 | Success |

### 实际发现并修正的错误

- 首轮把 W/S/A/D 当作模拟轴使用 BindAxisKey，触发 AxisKey.IsAxis1D 校验；改为标准数字键按下/松开绑定，焦点释放时复位。新增 PlayerInput 路由测试通过。
- 一次改用不存在的两参数 BindKey 重载导致 C2672；改为 FInputKeyBinding + KeyBindings 注册，实际重编译通过。
- UI 临时测试世界没有 LocalPlayer，正常 CreateWidget(PlayerController) 被引擎拒绝；测试改为初始化真实原生 WidgetTree 并执行真实按钮委托。实际游戏仍使用正常本地玩家创建 UI 的路径，没有删除生产检查。
- 真实角色蓝图在无基地的临时世界 BeginPlay 报 BaseGround 缺失；测试隔离禁用基地持久化并提供本地占位引用，不改真实基地。
- 最终还有旧引擎 API 弃用、非首选 MSVC / IncludeOrder 提示；不能声称整个编译/启动日志零警告。

## 4. PIE 完整操作（需要用户验证）

1. 保存当前工作并重启 UE，打开现有 L_Combat。保留 Director/DA_Combat_Phase6_Test 配置，点击“运行”。
2. 左下应出现“伴战与骑乘原型”面板。按钮区可滚动。点击“试装 / 切换：FireDragonTest”，场上切换为一个实体伴战，状态应为 ActiveCompanion_FireDragonTest。
3. WASD 移动再停止，观察伴战跟随。点击“召回伴战”，检查它回到人物附近。
4. 点击“F · 上龙 / 下龙”或在游戏获得焦点后按 F。人物应可见地站在占位龙上，主龙不再自行跟随；用 WASD 控制整组移动，镜头随之移动。此原型没有坐姿动画。
5. 移动至敌人附近，鼠标指向敌人并按住左键，应由龙发射有间隔的原弹丸；松开后停止。鼠标在控制面板上按按钮不应同时开火。
6. 按 Shift，观察短冲刺、撞墙受阻，以及冷却内不能连续冲刺。按 1 或点面板技能1，对照当前模块观察火/冰/风状态和冷却。当前默认单模块时，2/3 不会凭空产生技能。
7. 触发升级卡片，停留几秒观察人物、龙、敌人、弹丸、冲刺/技能时间停止；选卡后继续。暂停期间伴战操作不能生效。
8. 按 F 下龙，检查人物恢复 WASD、龙恢复跟随与自动攻击。靠墙或狭窄处没有安全下龙点应提示拒绝，不应穿墙落地。
9. 骑乘时点击“试装 / 切换：WindDragonTest”，应先安全下龙，再清理旧龙/弹丸并生成新的实体；独立火冰被动不应被试装覆盖。骑乘时点“卸下实体伴战”同样先下龙，原角色可继续操作。
10. 重新装备、上龙后完成一次胜利，再开新一局承受伤害直到失败。两种结果都要检查只结算一次、不再攻击、不遗留控制载体，点击“返回基地”到 L_Base。
11. 查看原建筑；有意保存并退出重进验证读档前先备份自己的 Saved/SaveGames。自动化存档哈希未变不能代替这一次真实 PIE 跳图和读档验收。

上述真人键鼠、GPU 渲染、面板可读性、平滑镜头手感、骑手姿态及实际 OpenLevel 返回没有在 NullRHI 下伪造为通过。

## 5. 范围与遗留边界

- 已落地伴战操作闭环、上下龙控制权、骑乘移动/冲刺/主动攻击和生命周期回归，不能称为正式骑乘动画/成品手感。
- 占位载体使用固定胶囊，原龙仍没有物种专属骨骼挂点。当前只支持既有角色和测试资源；复杂模型体型需后续适配。
- 采用地面 CharacterMovement，不提供自由飞行或新的龙种。普攻是原有目标辅助弹丸，尚未制作自由瞄准龙息。
- 试装只影响本局，未新增主力龙长期选择、骑乘状态或资源钱包存档。
- 强制生命周期清理（死亡/销毁/卸载）必须解除附着；若世界被外部操作改成完全封闭，可能使用当前位置完成强制恢复。普通用户下龙始终要求安全候选。
- 本阶段可以开始完整真人 PIE 验收；通过后再推进更精细的骑乘手感、模型/动画与碰撞规格，或后续龙族持久化任务。

## 6. 截图反馈修复：落地后不能下龙、缺失实体不能召回

2026-10-23 实际收到开阔地面的截图，但提示“周围没有安全下龙位置”。
原测试只有无地面/包围障碍情形，未覆盖载体落地后胶囊高度与骑手不同的组合。
新增两项真实回归，修改前均失败：
`Saved/Automation/MountGroundFix-Before/Custom_20261023_163524_0a258cdb/index.json`。

- 根因：载体半高88cm，测试模拟实际较高骑手半高96cm；载体落地中心约90cm，
  旧代码从该中心扫掠96cm胶囊，初始底部已深入地面，所有方向都被判阻挡。
  修复为从有地面间隙的高度扫向候选点，保留候选胶囊重叠与墙体扫掠检测。
- 召回原本仅能移动已存在的实体，缺失时直接返回false。现在记录本局上次选择的预设，
  卸下/销毁后可经原 SelectCompanion/EquipDragon 恢复并召回；从未选择时使用现有定义的默认候选。
  无有效配置、升级暂停、死亡或骑乘期间有明确反馈，不凭空创建配置。
- 伴战按钮文字改为在按钮内容宽度内填充并居中，减少截图中的过早逐字换行；最终布局仍需真人PIE检查。

最终 Editor 标准构建 Succeeded，Full 39项全部Success、0失败/用例警告/跳过，存档哈希未变：
`Saved/Automation/MountGroundFix/Full_20261023_164149_5ecc33e2`。
原 BlockedDismountAndCollision 包围障碍测试继续通过，未删除墙体安全校验。
本轮只重编Editor并运行Full；Game Target的成功记录是上一轮37项代码的结果，不冒充本次重编。
期间UI格式修复曾错误调用不存在的USizeBox.SetHorizontalAlignment，编译报C2039；已移除，
使用真正的UButtonSlot.SetHorizontalAlignment(HAlign_Fill)，重新构建和Full验证成功。

用户需要重新打开UE加载新DLL，再检查两条完整操作：
1. 开阔地面F上龙→行走/停下→F下龙，应成功；真正被墙包围仍应拒绝。
2. 选择一种测试龙→卸下实体→点“召回伴战”，应恢复同一种实体且只出现一条；销毁后也可恢复。

## 7. 第二次下龙反馈：控制器蓝图销毁回调生成额外角色

用户确认召回恢复正常；新截图显示模式已回并肩，但龙旁残留另一个人物，之后上龙反馈控制角色不一致。
只读检查项目控制器蓝图，确认 BP_TopDownController 的 ReceivePossess 给每个被控制 Pawn 的 OnDestroyed
绑定 OnDestroyed_Event；该事件会生成 PlayerClass 并再次 Possess。
临时骑乘载体也被这个通用重生逻辑绑定，下龙销毁它时会重生额外人物并抢走控制权。

此前测试使用实际角色蓝图搭配原生APlayerController，未覆盖控制器蓝图事件；
即便改用实际控制器，未初始化BeginPlay的临时世界也不会完整执行蓝图事件，不能认为那样的通过充分。
新增 ActualBlueprintControllerNoRespawn，完整初始化 BeginPlay，提供 PlayerStart 并使用实际两套蓝图。
修复前真实失败：正常下龙后角色数量从1变为2，PC.GetPawn不再等于原Player，后续上龙失败。
证据：`Saved/Automation/DismountController-Before/Custom_20261023_170608_8d68f89c/index.json`。

生产修复只在 Possess 临时载体之后调用 `Host->OnDestroyed.RemoveAll(PC)`，
仅移除该控制器对该临时载体的销毁回调。保留载体C++恢复流程、其他监听者及原角色的死亡重生绑定，
不修改/保存控制器uasset。回归验证两次正常下龙和一次载体销毁后都只有原人物，
控制器仍控制原Player，并断言原角色OnDestroyed仍包含控制器的OnDestroyed_Event。

测试收尾主动解绑测试原角色的重生回调再销毁测试控制器，避免测试清理误启动真实重生而在无基地世界产生警告。
一次测试编译把Sparse动态多播误用IsBoundToObject，已改为支持的Contains(Object,FunctionName)。
最终Editor构建Succeeded、Full40项成功、0失败/用例警告/跳过，现有玩家存档哈希保持一致：
`Saved/Automation/DismountControllerFix/Full_20261023_172122_f9e49ec7`。
本轮未重新构建Game Target；实际真人PIE结果仍需重新打开编辑器验证。

验收：结束旧PIE并重启编辑器，重新运行L_Combat，不沿用已生成重复人物的旧世界；
上龙→移动→下龙后只能出现原人物和龙，不应有第二道人形；WASD仍控制原人物，再次上龙应成功。
连续重复三次，同时确认卸下→召回仍正常。
