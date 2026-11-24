// Ported from the legacy mayhemheroes/csv-parser harness. The legacy version only
// constructed the reader (csv::parse_no_header) and never read a row, so almost no
// parsing happened; this one iterates every row and field of the returned reader.
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <exception>

#include <fuzzer/FuzzedDataProvider.h>
#include "csv.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::string str = provider.ConsumeRandomLengthString(1 << 16);
    csv::string_view sv{str};
    try {
        auto reader = csv::parse_no_header(sv);
        size_t total = 0;
        for (auto& row : reader) {
            for (auto field : row) total += field.get_sv().size();
        }
        (void)total;
        (void)reader.get_col_names();
    } catch (const std::exception&) {
        // parse_no_header reports malformed input by throwing (documented API).
    }
    return 0;
}
