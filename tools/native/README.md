# MR Studio native ace-midi patch

Base: timoncool/HOT-Step-CPP ee7286d14232b48ace3b6eed0fcf3048081ef0d1 (MIT).
`ace-midi-stem.patch` changes only engine/tools/ace-midi.cpp. JSON pins the patch
and complete patched source hashes; CI verifies both before compilation.

The optional --instrument canonical-role adds a teacher-forced program to each
chunk prelude and masks incompatible category tokens before argmax. It is a
native decoder constraint, not a post-hoc rename or learned embedding. Auto is
unchanged. No pitch transform/monophonic filter/accuracy guarantee is added.
--mrs-self-test checks the actual category mask and greedy selection without
weights for all canonical categories. The adapter also validates every raw role.

Generic SSE2 and AVX2/FMA/F16C/BMI2 engines are built from identical patched
sources; Studio gates AVX2 on CPUID and OS state. Runtime notices retain upstream
MIT/GGML/minimp3 notices and identify this patch; weights are never packaged.
