# P0-02A：伴战龙跟随与战斗基础完善

日期：2026-10-14。分支 main，起始 HEAD `3747188`。已读根目录 AGENTS.md、docs/Testing/README.md 和最新 GDD。

初始只有用户 AGENTS.md 的未提交编辑，已保留。未修改任何 uasset/umap、存档格式、玩家存档、地图或引擎文件；未提交或推送。

## 1. 原来已经实现的功能

- ALoongCombatDragon 已是现有伴战 Actor，使用占位球体/可选静态或骨骼资源。Configure 将能力数据传给已有 ULoongAutoAttackComponent。
- EquipDragon 已按玩家位置 + 世界坐标 FollowOffset 生成伴战；同 DragonId 替换，RemoveDragon 已清除装备、来源技能冷却和已附加元素。
- 跟随使用 VInterpTo、速度系数 5，在 Fighting 状态运行。普通移动跟随和停止后的单调收敛在修改前已经通过回归，本轮保留该算法。
- 自动攻击已按射程选择最近有效敌人，使用独立冷却；升级暂停时冷却不推进。
- 伴战弹丸命中已使用自身 DragonId 的元素模块，而英雄保留已有被动模块。原 P001A.CompanionElements 来源隔离回归保留。
- 原结算流程已批量清理敌人、弹丸和伴战。

## 2. 本次新增或修复的功能

- 增加 FollowRecoveryDistance（默认 2000cm，编辑器下限 100cm）：与跟随目标距离异常时立即恢复至玩家 + 原 FollowOffset；正常距离仍用原插值。
- 非有限 FollowOffset、无效恢复距离在装备/Configure 时使用安全默认值。非有限或非正帧间隔不推进跟随；玩家/Director 无效、死亡、正在销毁或暂停时不跟随/攻击。
- 卸下或替换伴战时取消该 Actor 已发出的弹丸并标记 consumed，保留其他龙和英雄弹丸；命中入口拒绝已注销的伴战来源，避免残留普通伤害。
- 直接 Destroy 或 EndPlay 也会停用攻击、解绑攻击 Director/OnShot，并注销对应装备、来源状态和在途弹丸。删除操作先移出登记数组，避免生命周期回调重入原迭代。
- 攻击选择增加健康组件、射程等有效性检查；弹丸对消失目标/组件增加检查。暂停或结束不继续攻击。
- 玩家在战斗中或升级暂停期间被移除时安全结算；结束/关卡卸载不操作失效玩家组件。
- 保留所有已有反射类、函数名和数据字段；只向能力结构追加恢复距离，不改保存资产。没有骑乘、控制权切换、新龙种、繁殖或新地图。

### 修改前的真实证据

修正测试生命周期用法后，五项伴战测试实际得到 2 成功、3 失败：
- 普通跟随收敛和目标/攻击冷却通过。
- 距离异常没有立即恢复。
- 卸下后来源弹丸仍保留，ResolveHit 将目标从 1000 降为 985，证明残留普通伤害。
- 直接销毁后装备、来源状态和弹丸仍有残留。

报告：`Saved/Automation/P0-02A-Before/Custom_20261014_205241_27ea23c6/index.json`。

最初新测试直接 Tick 已注销组件，引发 UE bRegistered 断言；这是测试写法错误，不是游戏中正常调度的崩溃证据。已改为检查注册状态。新增 EndPlay 测试也曾因未完整初始化临时世界而失败；按 InitializeActorsForPlay、NotifyBeginPlay、World.EndPlay(LevelTransition) 的引擎顺序修正后通过，没有放宽断言或生产校验。

## 3. 修改的源码及原因

| 文件 | 作用 |
|---|---|
| Source/Loong/LoongCombatDemo.cpp | 跟随恢复、有效性检查、来源登记、在途攻击取消、销毁/EndPlay 清理与玩家移除安全 |
| Source/Loong/LoongCombatDemo.h | 增加内部弹丸来源标记、生命周期覆盖和来源查询/清理方法，保留原接口 |
| Source/Loong/LoongCombatTypes.h | 兼容追加 FollowRecoveryDistance |
| Source/Loong/Tests/LoongCompanionTests.cpp | 六项真实伴战回归 |
| docs/Testing/README.md | 更新当前组数量和测试入口 |
| docs/P0-02A-Development-Report.md | 本报告 |

没有改共享测试 fixture 或增加测试框架。共用攻击/弹丸/Director 被修改，因此按 AGENTS 运行 Combat 后再执行 Full。

## 4. Editor 实际编译结果

PowerShell 7.6.7，运行器自动发现 UE 5.8.3 / CL 58210709、LoongEditor Win64 Development；实际工具链 MSVC 14.51.36260、Windows SDK 10.0.26100.0、引擎内 .NET 10。

```powershell
./Tools/Run-LoongTests.ps1 -Group Combat -ChangedModules Combat -ReportPath Saved/Automation/P0-02A
./Tools/Run-LoongTests.ps1 -Group Full -ChangedModules Combat -SkipBuild -ReportPath Saved/Automation/P0-02A
```

最终标准 Editor 构建成功，BuildExitCode=0，Build.log=Result: Succeeded。报告目录 `Saved/Automation/P0-02A/Combat_20261014_210742_fbf4d478`，实际 UBT 参数及工具链在 Build.log 中。Full 使用刚通过的同一份构建，不冒充重新编译。

编译存在已有引擎/工具链提示，不称为零警告。日志启动阶段也有引擎自有测试标签提示，需与本项目用例的测试警告区分。

## 5. 自动测试名称与真实结果

原生 Automation、NullRHI。最终 Combat：20 成功、0 失败、0 用例警告、0 跳过。已读取 summary.json、index.json 和 Build.log 确认，不只判断进程退出码。

| 新增测试（前缀 Loong.Combat.Companion.） | 实际行为 | Combat 结果 |
|---|---|---|
| FollowConvergence | 初始偏移、正常移动平滑、停止单调收敛、暂停/恢复 | Success |
| FarDistanceRecovery | 远距/玩家传送恢复、异常帧参数、无效偏移/范围回退 | Success |
| TargetAndAttackCooldown | 销毁/缺少生命组件目标排除、独立冷却、暂停、实际命中伤害 | Success |
| UnequipCancelsOwnedAttacks | 来源状态清理、取消旧弹丸、保留其他来源、同 ID 替换清理 | Success |
| DestroyAndSettlementSafety | 直接销毁注销、玩家死亡/被移除、战斗与升级暂停结束安全 | Success |
| DirectorEndPlayCleanup | 已 BeginPlay 世界的 LevelTransition EndPlay 清理 | Success |

既有 P001A.CompanionElements 来源隔离测试及其他 Combat 回归均 Success。
Full：28 成功、0 失败、0 用例警告、0 跳过，报告目录 `Saved/Automation/P0-02A/Full_20261014_211502_9175a8a0`。
包括网格/建造/存档、阶段生成、远程弹丸、升级、180 秒模拟和防重复结算，均未发现回归。
运行器每次的 SaveHashesUnchanged=true，现有三份 BaseSave_01 文件保持不变。
pre-commit 指定源码检查和 git diff --check 均通过，Git 暂存区保持原样。

## 6. 仍需要用户在 UE5 PIE 中验证的内容

以下未以 NullRHI 声称通过：

1. 重新打开项目，使用“内容浏览器”进入 L_Combat；在“大纲视图”检查 Director 的 Definition，保留现有三元素预设。
2. 点击“运行”，WASD 直行、斜行、急转，再停止；观察伴战逐渐追随并稳定，没有视觉抖动。FollowOffset 保持世界坐标含义，没有变成随朝向旋转的偏移。
3. 在运行中的“大纲视图”选择伴战 Actor，通过“细节”将位置移至离玩家数十米，再回到游戏；观察下一次有效战斗 Tick 恢复到玩家附近。
4. 靠近敌人观察自动弹丸、冷却和元素；升级选择时暂停，选择后恢复；结束战斗后不继续攻击。
5. 在 PIE“大纲视图”删除一个伴战，确认其弹丸/来源状态停止，其他来源保留；同 ID 卸下/替换的规则已自动化覆盖，不要求为验收新增蓝图逻辑。
6. 玩家死亡、胜利、点击“返回基地”，确认地图切换和基地原建筑/保存读取正常。这仍是真实地图验收，不能以临时 World.EndPlay 等同于用户 PIE travel。

当前是无碰撞占位 Actor 的位置跟随，没有导航绕障、正式动画或飞行系统；20m 恢复阈值是原型参数，仍需试玩确认。多个伴战的数据若使用相同 FollowOffset，可重叠；本轮保持已有队伍和测试预设规则，可通过定义配置分别调整偏移。

## 7. 是否具备进入 P0-02B / P0-02C 的条件

跟随/攻击/来源清理的 C++ 回归基础已具备，先完成上述 PIE 条目再标记本阶段验收通过。未提供 02B/02C 的详细任务定义，不能断言两个阶段已经全部准备就绪。

按 GDD 下一步可以规划骑乘与控制原型，但现有 ALoongCombatDragon 仍为 AActor、无受控移动/碰撞和骑手挂点，不能直接当作可 Possess 的 Pawn。后续需要明确控制/挂接方案；本轮没有提前进行该重构或承诺骑乘已可用。
