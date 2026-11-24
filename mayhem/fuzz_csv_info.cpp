// In-process libFuzzer driver for the legacy `csv-info` target (programs/csv_info.cpp).
// The legacy target ran the csv_info CLI on a file (`csv_info @@`). Run as a raw
// file-input target, Mayhem reported it crashing on every test case (including the
// committed seeds, which it handles cleanly under `docker run`), so the run could not
// start. This driver runs the SAME code: upstream's csv_info main(), compiled with
// -Dmain=csv_info_main, on a file holding the fuzz input. That reaches
// get_file_info(): the mmap reader, dialect guessing and the full row count.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

int csv_info_main(int argc, char** argv);

static std::string g_path;
static std::ostringstream g_sink;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    g_path = "/tmp/csv-info-fuzz-" + std::to_string(getpid()) + ".csv";
    std::cout.rdbuf(g_sink.rdbuf());  // the CLI prints a summary per input; discard it
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (g_path.empty()) LLVMFuzzerInitialize(nullptr, nullptr);
    FILE* f = fopen(g_path.c_str(), "wb");
    if (!f) return 0;
    if (size) fwrite(data, 1, size, f);
    fclose(f);
    std::string arg0 = "csv_info";
    char* argv[] = {&arg0[0], &g_path[0], nullptr};
    try {
        csv_info_main(2, argv);
    } catch (const std::exception&) {
        // get_file_info reports malformed input by throwing (documented API).
    }
    g_sink.str("");
    g_sink.clear();
    unlink(g_path.c_str());
    return 0;
}
