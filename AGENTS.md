
1.执行前先检查问题有没有错误前提、逻辑跳跃或信息缺失。
2.不要一味迎合，要独立判断，区分事实、预测和主观观点。
3.涉及数字、人物和结论时，尽可能核实
4.如果发现我说得不对，直接指出，并说明依据、风险以及更合适的解释。
5.同时主动提醒我可能忽略的变量、成本和偏差

# Loong development and verification

- Preserve existing Git changes and player saves. Never edit Unreal binary assets as text.
- Read `docs/Testing/README.md` when changing C++ or test infrastructure.
- Use the installed PowerShell 7 (`pwsh`) for test scripts; they require version 7 or later.
- Record the changed modules in the report and with `-ChangedModules` when running tests.
- After C++ changes, compile the Editor target and run the relevant native Automation tests:
  `./Tools/Run-LoongTests.ps1 -Group <group> -ChangedModules <modules>`.
  The runner discovers the project's engine. Supply `-EngineRoot` if discovery cannot find it.
- Groups: Quick, Combat, Elements, Grid, Building, Persistence, Camera, Full.
  Combat/element changes require Combat or Elements; placement requires Building;
  saves require Persistence plus Building. Shared/test-runner changes require Full.
- Use `-SkipBuild` only after a successful build of the current C++ source. It does not prove compilation.
- Analyze actual build logs and `index.json` on failure, fix the cause, then rerun affected checks.
  Never accept retries without a fix or use process exit code alone to claim success.
- New tests must be deterministic, own their transient world/state, and clean up unique test save slots.
- Report which tests ran, their results, and the report directory. Mark rendering, input feel,
  riding, Niagara quality and actual PIE map travel as manual checks until observed.
- Use `pre-commit run -a` for formatting. Keep build/test output under ignored `Saved/` by default.
