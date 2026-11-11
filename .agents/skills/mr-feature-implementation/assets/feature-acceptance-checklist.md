# Feature Acceptance Checklist

- [ ] User-visible behavior matches the feature contract.
- [ ] All affected layers are classified.
- [ ] There is one clear source of truth.
- [ ] UI does not own business/domain state improperly.
- [ ] Engine/domain behavior works independently of UI details.
- [ ] Undo/redo decision is explicit.
- [ ] Persistence decision is explicit.
- [ ] Save/load round-trip passes when required.
- [ ] Older/default state remains safe when required.
- [ ] Studio/Live sharing decision is correct.
- [ ] Realtime-sensitive code passed `mr-realtime-audio-safety` review when applicable.
- [ ] Relevant automated tests exist/pass.
- [ ] `mr-studio-build-test` validation passes.
- [ ] No accidental unrelated architectural changes.
- [ ] Documentation/issues/roadmap updated if required.
- [ ] Final status is FEATURE READY or explicitly READY WITH MANUAL CHECK.
