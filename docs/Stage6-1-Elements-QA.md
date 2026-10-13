# 第六阶段第一批：多敌人数据与元素基础

日期：2026-10-13。稳定回退提交 ed3fed8；git fetch origin main 后本地/远程一致、起始工作区干净。未提交、未修改uasset/umap、未改基地持久化。

新增 LoongElementComponent.h/.cpp、Tests/LoongElementTests.cpp。
修改 LoongCombatTypes.h、LoongCombatDemo.h/.cpp。

## 数据兼容

原 Enemy 配置保留。FindEnemy 优先该配置的 EnemyId（默认 MeleeTest），不忽略旧资产对生命/伤害/经验的修改。EnemyDefinitions 增加 FastTest/RangedTest。
SpawnPhases 以 StartSeconds、IntervalMultiplier、BatchSize 和 EnemyId/Weight 描述阶段；不在Director算法硬编码类型或阶段。
目录为空/阶段无有效候选时回退原 Enemy；未知显式生成ID返回空。
原三选一、180秒胜利、奖励一次性结算不改。未开发Boss。

## 初始测试内容

MeleeTest：原近战；FastTest：25生命/240速度/橙色小体型；RangedTest：35生命/110速度/紫色，距离约650cm，射程1000cm，每2秒发射可躲避直线弹丸。
默认阶段：0秒近战；60秒混合；120秒增加混合批次。数值可在CombatDefinition编辑，最大存活敌人仍受MaxEnemies控制。
双方弹丸总数默认120受MaxProjectiles限制，不每帧扫描全图。

## 元素组件

FLoongElementEffect：EffectId/Element/Duration/TickInterval/MaxStacks/ApplicationCooldown/RefreshRule/DamagePerTick/SlowPerStack/FreezeAtStacks/FreezeDuration/Radius/DisplacementPerTick/VisualEffect。
FLoongElementAbility：SkillId/TriggerChance/TriggerCooldown/Effect；同一DragonId可携多个模块。
状态按来源DragonId+EffectId分组；不同元素和不同来源并存，同来源同效果按上限叠加。有效参数必须有限且尺寸/时间非负。
RefreshDuration重置持续时间；KeepDuration仅增加层数；ExtendDuration叠加时延长至Duration*MaxStacks上限。
火：间隔伤害乘层数。冰：取最强减速，累计成功施加到阈值短冻结。风：围绕标记目标范围周期伤害，正位移击退/负位移牵引到施法来源位置，位置限制在测试场地。
战斗暂停时状态时间、持续伤害、冻结、敌人攻击和技能冷却暂停；死亡/结算/卸下来源龙清理。
现有龙额外直接伤害逻辑保留，新增模块由ApplyDragonElements调用，实际效果处理独立于人物攻击。
范围伤害使用敌人注册列表快照，避免同步死亡注销迭代失效；单次大DeltaTime最多64次补 tick，长卡顿不保证完整补算。

## 测试预设启用

默认不替换原WindTest，也不自动开启新模块。CombatDefinition 的 ElementDragonPresets 含 FireDragonTest/IceDragonTest/WindDragonTest。
勾选 bEquipElementTestPresets 即同时装备测试配置；无需蓝图执行图连线。火/冰被动附加，风附加旋风效果且有伴战龙。
未配置Niagara时伤害/控制仍运行，但不会声称已完成燃烧/冻结/旋风的正式视觉效果。

## A/B接口

保留 LoongCombatArtPalette。战斗定义已有Mesh/SkeletalMesh/Material，新增敌人VisualScale/PlaceholderTint。
元素VisualEffect、敌人AttackEffect/DeathEffect使用软Niagara引用；事件OnElementChanged提供效果ID/层数，原Health.OnDamaged/OnDeath及Attack.OnShot保留。
Maya FBX须统一厘米/Z-up、骨骼及动画匹配；换模型/材质/特效通过定义资产；不需要改伤害或AI算法。骨骼动画播放资源仍由既有AnimationBlueprint/美术配置负责，换Mesh不会自动转换不兼容骨骼。
软Niagara第一次施加可同步加载，当前测试规模可用；正式资源密集时应预加载，不能声称没有首次加载峰值。

## 验收

重启UE，现有L_Combat不更换GameMode。若使用配置资产，打开Director的Definition，确认EnemyDefinitions/SpawnPhases新字段；若旧资产序列化为空，可在新测试Definition使用C++默认目录，保留原资产回退。
元素测试只需勾选 bEquipElementTestPresets。
0–60秒近战；60秒后更快小怪及保持距离远程怪、紫色远程弹丸；120秒后压力提高。升级期间全暂停；选择后恢复。
观察燃烧额外跳伤、减速/冻结、风范围伤害/牵引；测试死亡/结算后停止；返回基地无建造/存档退化。
实际构建与测试记录见本报告后续验证结果。

## 最终验证记录

- LoongEditor Win64 Development：真实UBT成功，Saved/Logs/LoongPhase6EditorBuild.log。
- Loong Win64 Development：真实UBT成功，Saved/Logs/LoongPhase6GameBuild.log。
- UnrealEditor-Cmd -nullrhi 实际跑 Loong 自动化：7成功、0失败、0警告、0跳过，退出码0。
- 基地网格3项、建造交互、存档、第五阶段LoopAndAbilities与新EnemiesAndElements全通过。
- 测试报告 Saved/Automation/Phase6/index.html / index.json；日志 Saved/Logs/LoongPhase6Tests.log。
- 编译有本地UE/Niagara弃用声明和非首选MSVC警告；不等同于编译0警告。
- 未执行本批人工PIE、GPU性能或目标硬件30怪帧率测试；无正式美术视觉验收结论。
- git diff --check通过。现有BaseSave_01及辅助文件保留，自动化使用独立测试槽，测试文件正常清理。

新增Niagara用户参数：User.Duration、User.Radius、User.Stacks。须由B线Niagara资产声明并使用，空引用不影响玩法。
风范围伤害触发升级时会停止后续伤害循环。来源/效果ID相同但元素类型冲突时拒绝施加。
单次效果视觉加载、状态快照分配、每个风效果扫描注册敌人是未来性能测量重点；本批没有声称已达到PC/移动帧率。

## 最少人工操作（测试新玩法）

1. 重启UE，打开L_Combat，保持LoongCombatGameMode和原角色设置。
2. 如果关卡已有LoongCombatDirector，打开其Definition指向的数据资产。新字段 Enemy Definitions、Spawn Phases、Element Dragon Presets 可配置。
3. 如没有可配置Director：从C++类放置一个LoongCombatDirector；创建一个新的数据资产，选择LoongCombatDefinition（例如DA_Combat_Phase6_Test）；指派Director.Definition。不要覆盖第五阶段配置。
4. 新数据资产含默认快速/远程目录和阶段。勾选 Equip Element Test Presets 开启三元素测试模块；默认关闭以保留旧龙配置。
5. PIE测试0/60/120秒阶段、橙色快速怪、紫色远程怪/可躲避弹丸、燃烧跳伤、冰减速/冻结、风牵引及周围伤害。
6. 升级时计时/攻击/状态暂停，选择后恢复；结算不重复；返回基地检查建造和存档。
7. 正式Niagara未配置时无需制作素材即可验证规则，但视觉上不必出现火焰/冰壳/旋风。UI仍复用原三选一。

本批没有Boss、九项强化扩展、孵化融合、正式动画或材质制作。远程为开放场地保持距离和直线弹丸，未新增导航绕障；风推动使用位置位移，不保证复杂场景墙体扫掠。
建议PIE通过后提交消息：[feature]: add multi-enemy phases and element effects。
