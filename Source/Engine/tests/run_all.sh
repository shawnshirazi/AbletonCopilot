#!/usr/bin/env bash
# Builds and runs every Engine unit test with a bare C++17 compiler - no
# JUCE, no Xcode. Usage: Source/Engine/tests/run_all.sh [test_name ...]
set -u
cd "$(dirname "$0")/.."
CXX="${CXX:-clang++}"
OUT="${TMPDIR:-/tmp}/abletoncopilot-engine-tests"
mkdir -p "$OUT"
tests=("$@")
[ ${#tests[@]} -eq 0 ] && tests=($(cd tests && ls test_*.cpp | sed 's/\.cpp$//'))
failed=0
for t in "${tests[@]}"; do
    if ! "$CXX" -std=c++17 -O1 -I. "tests/$t.cpp" ./*.cpp -o "$OUT/$t" 2> "$OUT/$t.log"; then
        echo "BUILD FAIL $t (see $OUT/$t.log)"; failed=1; continue
    fi
    if result=$("$OUT/$t" 2>&1); then
        echo "ok   $t: $(echo "$result" | tail -1)"
    else
        echo "FAIL $t:"; echo "$result" | grep FAIL | head -20; failed=1
    fi
done
exit $failed
