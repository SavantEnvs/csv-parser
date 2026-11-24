// Reader -> csv::make_csv_writer -> reparse round trip (the programs/round_trip logic):
// exercises the writer's quoting/escaping on parser-produced fields, then re-parses the
// written CSV. No equality assertion: the writer is not documented as lossless for every
// input the permissive reader accepts (bare CR, NUL, trimmed whitespace).
#include <stdint.h>
#include <stddef.h>
#include <sstream>
#include <string>
#include <vector>
#include "csv.hpp"

static std::vector<std::vector<std::string>> read_all(const std::string& text, csv::CSVFormat fmt) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream in(text);
    csv::CSVError error;
    auto reader = csv::CSVReader::try_open(in, error, fmt);
    if (!reader) return rows;
    csv::CSVRow row;
    while (reader->try_read_row(row, error)) {
        std::vector<std::string> fields;
        for (auto field : row) fields.emplace_back(field.get_sv());
        rows.push_back(std::move(fields));
    }
    return rows;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > 65536) return 0;
    const bool quote_minimal = data[0] & 1;
    const char delim = ",;\t|"[(data[0] >> 1) & 3];
    const std::string text(reinterpret_cast<const char*>(data + 1), size - 1);

    csv::CSVFormat fmt;
    fmt.delimiter(delim).no_header().variable_columns(csv::VariableColumnPolicy::KEEP);
    fmt.threading(false);
    auto rows = read_all(text, fmt);

    std::stringstream out;
    auto writer = csv::make_csv_writer(out, quote_minimal);
    for (const auto& r : rows) writer << r;
    auto again = read_all(out.str(), csv::CSVFormat().delimiter(',').no_header()
        .variable_columns(csv::VariableColumnPolicy::KEEP).threading(false));
    (void)again;
    return 0;
}
