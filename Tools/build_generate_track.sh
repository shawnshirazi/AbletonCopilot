#!/usr/bin/env bash
# Builds the standalone generate_track command-line tool (no JUCE, no
# Xcode - any C++17 compiler; on macOS the Xcode command line tools'
# clang++ is enough). Output: build/generate_track at the repo root.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
"${CXX:-clang++}" -std=c++17 -O2 -ISource/Engine Tools/generate_track.cpp Source/Engine/*.cpp -o build/generate_track
echo "built build/generate_track"
