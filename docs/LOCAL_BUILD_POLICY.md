# Local development and build policy

User instructions, recorded 2026-10-05:
- Development, source commits, configure/build/tests and packages stay local on Windows.
- GitHub is used for issues/plans and documentation only. Push source or merge only
  on the user's explicit request. Do not use GitHub Actions for these builds.
- No mobile work in the active JUCE migration.
- Nuro Audio project-state restore is user accepted. Do not run special installed
  Nuro compatibility/state checks on every build. Repeat only when the user requests
  it or reports a new Nuro issue. Keep ordinary synthetic VST3 regression tests.
- TH-U project-state/sound restore is user accepted on 2026-10-05. Do not run
  special installed TH-U editor/state/DSP comparisons on routine builds. Repeat
  only on an explicit request or a newly reported TH-U-specific issue. Keep ordinary
  synthetic plugin regression coverage; new generic preset workflow tests use fixtures.