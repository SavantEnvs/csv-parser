// Upstream's fuzz/boundary.cpp, run with its `compact` recipe flag always set.
//
// Why: a non-compact recipe synthesizes ~11 MB of CSV and parses it five times
// (stream/mmap x threaded/unthreaded + baseline); under ASan+UBSan that is ~8 s per
// execution, so the target cannot iterate at all inside a per-input timeout. The
// compact flag (bit 0 of recipe byte 2) is a harness knob, not CSV input: it moves the
// same synthesized boundary to csv::internals::CSV_CHUNK_SIZE_FLOOR instead of
// CSV_CHUNK_SIZE_DEFAULT, keeping every other recipe option (displacement, preview
// offset, custom tail, unquoted/escaped/NUL placement) fuzz-controlled (~0.5 s/exec).
#include <cstdint>
#include <cstddef>
#include <cstring>

#define LLVMFuzzerTestOneInput csv_upstream_boundary_fuzz
#include "../fuzz/boundary.cpp"
#undef LLVMFuzzerTestOneInput

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Upstream's own bounds (fuzz/boundary.cpp: `if (!size || size > 64) return 0;`).
    if (!size || size > 64) return 0;
    uint8_t recipe[64] = {0};
    std::memcpy(recipe, data, size);
    // Recipes shorter than 3 bytes carry no flag byte; give them one so they stay compact.
    const size_t n = size < 3 ? 3 : size;
    recipe[2] |= 1;
    return csv_upstream_boundary_fuzz(recipe, n);
}
