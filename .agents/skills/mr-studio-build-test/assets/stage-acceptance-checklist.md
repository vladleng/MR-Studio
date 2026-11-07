# Stage acceptance checklist

Use **only** before closing an MR Studio / MR Live major stage or acceptance/release substage, **not** after ordinary L0/L1a/L1b cosmetic edits. For L1 use the skill's fast-path criteria and mark the full acceptance matrix N/A.

- [ ] Acceptance criteria are identified.
- [ ] Required validation level selected.
- [ ] Configure passes.
- [ ] Required targets build.
- [ ] Required automated tests pass.
- [ ] Runtime smoke check passes when applicable.
- [ ] Save/reload passes when persistent state changed.
- [ ] Audio/device checks pass when relevant.
- [ ] Hardware-only checks are either passed or explicitly marked as pending.
- [ ] No unexplained new warnings/assertions/crashes.
- [ ] Adjacent regression scope checked.
- [ ] Final status is READY or explicitly READY WITH MANUAL CHECK.
- [ ] Stage is not closed while a required check is FAIL or BLOCKED.
