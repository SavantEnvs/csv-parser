#!/usr/bin/env bash
# mayhem/build.sh — build csv-parser's fuzz targets, the csv_info CLI target and the
# functional oracle. Runs as `mayhem` in /mayhem; re-runnable and offline (no deps
# beyond the base toolchain + pthreads). Everything is written under build/ (gitignored).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
cd "$SRC"

# The same configuration upstream's CMake gives the `csv` target (threads on,
# std::from_chars float parsing), C++20 as upstream's default.
DEFS="-std=c++20 -DCSV_ENABLE_THREADS=1 -DCLASSIFY_SCALAR_USE_STD_FLOAT_FROM_CHARS=1 -Iinclude -Itests -pthread"
LIB_SRCS="$(ls include/internal/*.cpp include/internal/parser/*.cpp)"
OUT=build/mayhem
BIN=$OUT/bin
mkdir -p "$BIN" "$OUT/fuzz" "$OUT/cli" "$OUT/test"

# compile_set <objdir> <flags...>: compile every library TU into <objdir>
compile_set() {
  local dir="$1"; shift
  local flags="$*"
  printf '%s\n' $LIB_SRCS | xargs -P "$MAYHEM_JOBS" -I{} sh -c \
    'o="'"$dir"'/$(echo "{}" | tr / _).o"; '"$CXX"' '"$flags"' -c "{}" -o "$o"'
}

# 1) The library, instrumented for fuzzing (sanitizers + SanCov + DWARF 3).
FUZZ_FLAGS="$DEFS -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
compile_set "$OUT/fuzz" $FUZZ_FLAGS
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

# 2) Harnesses: upstream's fuzz/*.cpp + ours in mayhem/. Each linked twice:
#    libFuzzer binary and run-once standalone reproducer.
build_harness() {
  local name="$1" src="$2"
  $CXX $FUZZ_FLAGS -c "$src" -o "$OUT/$name.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE -pthread "$OUT/$name.o" \
    "$OUT"/fuzz/*.o "$OUT/lsan_off.o" -o "$BIN/$name"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -pthread "$OUT/$name.o" "$OUT/standalone_main.o" \
    "$OUT"/fuzz/*.o "$OUT/lsan_off.o" -o "$BIN/$name-standalone"
}
pids=()
build_harness csv_fuzz_stream        fuzz/stream.cpp & pids+=($!)
build_harness csv_fuzz_differential  fuzz/differential.cpp & pids+=($!)
build_harness csv_fuzz_boundary      mayhem/fuzz_boundary_compact.cpp & pids+=($!)
build_harness fuzz_parse_no_header   mayhem/fuzz_parse_no_header.cpp & pids+=($!)
build_harness fuzz_row_json          mayhem/fuzz_row_json.cpp & pids+=($!)
build_harness fuzz_dataframe         mayhem/fuzz_dataframe.cpp & pids+=($!)
build_harness fuzz_roundtrip         mayhem/fuzz_roundtrip.cpp & pids+=($!)
# csv-info: upstream's csv_info CLI main() driven in-process (see mayhem/fuzz_csv_info.cpp)
( $CXX $FUZZ_FLAGS -Dmain=csv_info_main -c programs/csv_info.cpp -o "$OUT/csv_info_main.o" &&
  $CXX $FUZZ_FLAGS -c mayhem/fuzz_csv_info.cpp -o "$OUT/fuzz_csv_info_drv.o" &&
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE -pthread "$OUT/fuzz_csv_info_drv.o" \
    "$OUT/csv_info_main.o" "$OUT"/fuzz/*.o "$OUT/lsan_off.o" -o "$BIN/fuzz_csv_info" &&
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -pthread "$OUT/fuzz_csv_info_drv.o" "$OUT/csv_info_main.o" \
    "$OUT/standalone_main.o" "$OUT"/fuzz/*.o "$OUT/lsan_off.o" -o "$BIN/fuzz_csv_info-standalone" ) & pids+=($!)
for p in "${pids[@]}"; do wait "$p"; done

# 3) The csv_info CLI itself (used by test.sh's golden check): sanitized, no SanCov.
CLI_FLAGS="$DEFS -O1 $SANITIZER_FLAGS $DEBUG_FLAGS"
compile_set "$OUT/cli" $CLI_FLAGS
$CXX $CLI_FLAGS programs/csv_info.cpp "$OUT"/cli/*.o "$OUT/lsan_off.o" -o "$BIN/csv_info"

# 4) Functional oracle with the project's normal flags (no sanitizers).
TEST_FLAGS="$DEFS -O2 $COVERAGE_FLAGS"
compile_set "$OUT/test" $TEST_FLAGS
$CXX $TEST_FLAGS mayhem/oracle.cpp "$OUT"/test/*.o -o "$BIN/csv_oracle"
