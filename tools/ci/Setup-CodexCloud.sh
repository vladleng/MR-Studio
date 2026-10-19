#!/usr/bin/env bash
set -euo pipefail
# Linux development surface only; Windows product validation belongs to Actions.
command -v cmake
command -v c++
cmake -S . -B build/cloud-core -DCMAKE_BUILD_TYPE=Release \
  -DMRS_BUILD_ASIO=OFF -DMRS_BUILD_VST3=OFF -DMRS_BUILD_ARA=OFF -DMRS_BUILD_JUCE_UI=OFF
printf '%s\n' 'Core configured. Build only affected targets; do not run full CTest by default.'
