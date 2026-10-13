# Loong 第四阶段基地持久化交付记录

日期：2026-10-12；稳定回退 HEAD：3901c2e，第三阶段：c98e5e7。

## 文件与架构

新增 LoongBaseSaveGame.h、LoongBaseSaveSubsystem.h/.cpp、Tests/LoongBasePersistenceTests.cpp。
修改 LoongBuildingSystemComponent.h/.cpp、LoongBuildingInstanceComponent.h。
无资产、地图、旧 BP 执行图修改；未提交 Git。

ULoongBaseSaveGame 保存 SaveVersion=1、SaveSlotName、MapIdentifier、Buildings。
FLoongSavedBuilding 保存 InstanceGuid、BuildingId、完整世界 Transform、State。
State 含 StateVersion=1 与 FName/FString 字段扩展容器，供将来住户/生产等使用，不含 UObject 引用。
ULoongBaseSaveSubsystem 是 GameInstance 持久化服务，组件仍为唯一建造管理器。

## 生命周期

组件 BeginPlay 自动注册。默认 bEnableBasePersistence=true；Subsystem 默认 bAutoLoad=true、bAutoSave=true、AutoSaveDelay=0.5。
默认槽 BaseSave_01、用户索引 0。地图 ID 使用去掉 PIE 前缀的关卡包路径，可由 BaseIdentifierOverride 明确指定。
新建/确认移动/确认拆除通知更新已确认快照，0.5 秒防抖写盘。
预览、移动未确认、Esc 取消、拆除取消和恢复不触发写盘。手动 SaveBase 也保存已确认快照。
组件 EndPlay 刷新待保存快照，解绑事件；角色重生接管同基地的有效实例身份，避免新旧组件同时管理。

## 恢复与安全

先核对文件摘要，再使用 UE 原生 SaveGame 内存反序列化；随机损坏数据不会直接进入 UE 反序列化。
验证版本、槽、地图、已知定义、有效唯一 GUID、有限 Transform、正常旋转和正缩放，以及扩展状态版本。
全部记录验证后先生成暂存正式 Actor；全成功才清理旧托管实例。中途失败清理暂存并保留原基地。
恢复保持存档 GUID 与 Transform；反复加载不叠加实例。恢复期间不广播正常保存事件。
失败加载打开写回保护，后续自动保存/手动保存拒绝；成功加载或显式 NewBase 后解除。
不猜测旧蓝图/手工 Actor 类型；BaseGround、角色、装饰不清理。

## API 与命令

SaveBase / LoadBase / HasSave / DeleteSave(bool confirmed) / NewBase(bool confirmed) / ClearBase(bool confirmed)。
运行时控制台：

```text
Loong.Base status
Loong.Base save
Loong.Base load
Loong.Base new CONFIRM
Loong.Base delete CONFIRM
```

status 在输出日志显示每栋 ID、GUID 和 Transform。
new：只清空托管运行时建筑，不删除磁盘槽、不立即覆盖；以后明确 save 或新操作自动保存才写入新布局。
delete：明确删除槽/辅助文件，保留运行时建筑，并锁住自动写回；new 或成功 load 后解除保护。
移动/拆除弹窗期间拒绝 load/new/delete，避免丢失编辑事务。

## 磁盘与版本

使用 UGameplayStatics SaveGameToMemory / SaveDataToSlot / LoadDataFromSlot / LoadGameFromMemory；这是 UE 标准 SaveGame 后端，不自建路径或资源管理器。
编辑器通常 Saved/SaveGames：BaseSave_01.sav、BaseSave_01_Integrity.sav（20 字节 SHA1）、BaseSave_01_Backup.sav（上次主文件字节备份）。
打包使用 UE 平台 SaveGame 目录，不假定可执行目录可写。
摘要用于意外损坏检测，不是安全加密或恶意篡改防护。
写入前备份旧文件；备份失败拒绝覆盖；主写入失败报告并尝试恢复旧字节/摘要。
主文件和摘要不是跨文件原子事务：断电中间状态可能需人工备份恢复，程序会拒绝不匹配并保护文件，不自动猜测修复。
版本 1 只接受版本 1；未来必须加入明确迁移函数后才能读取旧版本，不静默覆盖不支持版本。

## 实际自动化与构建

LoongEditor Win64 Development、Loong Win64 Development：真实 UBT 成功。
UnrealEditor-Cmd -nullrhi 运行 Loong.Building：5 成功、0 失败、0 警告、0 跳过，退出码 0。
包含前三阶段网格/交互回归与新增存档 RoundTripAndFailures。
新增测试验证空/一栋/四种类型存档、GUID/类型/Transform 往返、未确认移动快照、确认移动、取消拆除、拆除后三栋、重复加载、缺失定义/重复 GUID/版本/地图、不完整恢复回滚、损坏二进制防护、防抖保存、退出刷新、重生重新绑定、实际磁盘写入失败。
使用随机 Loong_Automation_<GUID> 独立槽，测试生成文件已清理；未触碰玩家默认槽。
日志 Saved/Logs/LoongStage4EditorBuild.log、LoongStage4GameBuild.log、LoongStage4Tests.log。
报告 Saved/Automation/Stage4/index.json / index.html。

## 最少编辑器操作与 PIE/Standalone 验收

重新打开 UE；无需新增蓝图、节点或 Widget。原建造组件的 Enable Base Persistence 默认开启。
原 BuildGroundTrace、菜单、材质、旧输入停用状态保持。

1. PIE 开始，无存档时场景不清空。建造 1/2/3/4 各一栋。
2. 调整其中一栋位置和旋转，拆除另一栋，等待 0.5 秒；Loong.Base status 记录剩余三栋 GUID/Transform。
3. 正常结束 PIE，再次运行。应准确恢复三栋，移动位置与方向相同。
4. Loong.Base load 连续两次，建筑总数仍三栋。
5. 恢复建筑可选中、移动、Esc 取消和拆除；取消移动后退出重进保持原位置。
6. 移动中执行 Loong.Base save，再取消，重进不出现未确认位置；Loong.Base load 应拒绝未完成编辑。
7. 无效放置、拆除取消、点击 UI 不产生存档错误。
8. Standalone 正常退出并重新启动，重复核心三栋验收；不使用任务管理器强杀测试正常退出。
9. WASD、镜头、菜单、四尺寸/相邻/边界/旋转回归。
10. new/delete 都必须带 CONFIRM；危险命令只在可舍弃测试布局验证。new 后未 save 前仍可 load 原盘文件。

## 已知限制

人工 PIE/Standalone/打包跨进程验收未执行。测试提交移动走同一内部事务完成函数，真实鼠标地面命中仍依赖人工运行验收。
同步小布局写盘可阻塞帧；当前适合小型基地，后续大数据改为 UE 异步接口。
恢复采用事务式重建，GUID 保持但 Actor 指针变更；未来龙族/生产对象引用应使用 GUID。
恢复不重新格吸附、不按新基地形状重新计算占地；定义/地形改变后需要明确版本迁移策略。
不实现网络多人、经济、龙族、完整存档升级和旧 BP 导入。
当前只支持单活动基地/默认槽；同槽不同地图拒绝加载，配置新基地时需独立槽或显式稳定 ID。

建议人工验收后提交：[feature]: add base save and load system。

## 存档操作 UI 补充

新增 LoongBaseSaveWidget.h/.cpp，原生 C++ 常驻右上角中文面板。无需控制台命令。
按钮：保存、读档、刷新状态、清空基地、删除存档；明确显示槽名称、文件存在状态和操作成功/失败。
读档、清空、删除弹出确认窗口；Esc 取消。移动/拆除待确认时操作按钮禁用。
UI 点击与确认状态加入组件输入互斥；按钮触发后抑制同一点击继续执行场景动作。
0.5 秒刷新界面状态，避免每帧检查磁盘。自动保存成功也会显示结果。
新增反射 Widget 完成 Editor/Game 构建；原有自动化回归 5 项通过。新面板视觉和实际鼠标交互需 PIE 验收。
最少操作：重启 UE → PIE → 右上角保存 → 确认显示保存成功 → 读档并确认 → 验证布局；危险按钮只在可舍弃布局测试。
