# 第三阶段建造交互交付记录

日期：2026-10-12。回退提交：c092268。

## 实现

继续使用 ULoongBuildingSystemComponent。普通/选中/新建/移动/拆除确认状态由 GetInteractionState 暴露。
Delete 或操作面板拆除按钮开启居中确认 UI；取消/Esc 不修改建筑。删除仅接受当前管理组件拥有且具备有效 BuildingId、InstanceId 的实例组件。
左侧建造菜单宽 280 UMG 单位，高 520 单位，随默认 UMG DPI 缩放；支持分类筛选、滚动、选中状态。
右侧选中操作面板提供移动/拆除/取消选择。UI 悬停阻止场景点击；关闭 UI/确认移动后抑制同一次点击直到释放。
保存并恢复原材质、Overlay 和 Custom Depth/Stencil；可见反馈使用临时材质替换，不依赖不存在的后处理描边。
每次生成附加 FGuid；生成、移动、拆除提供数据通知（ID、类型、Transform）。未实现存档或费用返还。

## 验证

LoongEditor Win64 Development、Loong Win64 Development：UBT 成功（以 Saved/Logs/LoongStage3*Build.log 为准）。
实际 UnrealEditor-Cmd 自动化执行：Loong.Building，4 成功、0 失败、0 跳过、0 警告，退出码 0。
报告：Saved/Automation/Stage3/index.json、index.html；日志：Saved/Logs/LoongStage3Tests.log。
网格测试 3 项 + World 生命周期测试 1 项。生命周期测试包含选择、模式、移动取消/提交内部路径、GUID 保留、自碰撞、拆除取消/重复确认、占地释放、外部销毁清理；扩展了多尺寸四方向相邻/穿透检测。
尚未进行本批人工 PIE；无渲染命令行测试无法验证 UI 外观和真实点击焦点、移动确认的鼠标地面命中路径。

## 最少编辑器步骤

重新打开项目；保持原组件和旧 BP_BuildManager 输入停用。无需新 Widget 蓝图、无需连接节点。
Selected Building Material 可保留当前配置；为空回退到 PreviewValidMaterial。
本次权限要求仅编辑当前运行中 C++ 生成并注册的建筑；手工摆放同类 Actor 不自动纳入编辑。

## PIE 清单

- B 开关菜单，分类筛选、滚动、数字键、选中条目颜色。
- 菜单点击不生成建筑；空白游戏区域能正常建造；WASD/滚轮正常。
- 各尺寸四方向旋转、贴边相邻、穿透禁止、越界禁止。
- 普通左键选中，右侧显示名称/分类/尺寸；点击空地恢复材质并隐藏面板。
- 面板移动/M，R，合法确认，非法拒绝，Esc 恢复；Actor 和 GUID 保留。
- Delete/按钮确认显示名称；取消/Esc 保持原样；确认一次删除，重复点击安全；原地重新建造。
- 确认期间 B/M/R/数字键和场景点击不能操作；取消后可继续。
- 角色、基地、装饰物无法拆除；组件退出/目标外部销毁后 UI 清理。
- 不同窗口分辨率检查 280 宽面板和居中确认；PIE 的 Esc 默认退出绑定可能需要禁用以测试取消。

## 限制与版本整理

UI 当前为英文测试样式；图标/说明/解锁数据已预留，费用不扣除。
多玩家、网络复制、存档和旧建筑导入未实现。矩形地面算法仍不支持非矩形精确轮廓。
只提交本次 Source 变更和本报告；工作区已有 DefaultEngine.ini、BP_TopDownCharacter 和 M_PreviewHighlight 不自动提交。
建议提交消息：[feature]: add building demolition and interaction panels
