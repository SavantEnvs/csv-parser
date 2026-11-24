// csv::parse() -> row iteration -> CSVRow::to_json / to_json_array, row[name],
// CSVField typed accessors and vector<string> conversion. Uses the exception-based
// public API (stable across the project's history), so throws are expected and caught.
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <exception>
#include "csv.hpp"

static csv::CSVFormat pick_format(uint8_t b) {
    csv::CSVFormat fmt;
    switch (b & 3) {
        case 0: fmt = csv::CSVFormat::guess_csv(); break;
        case 1: fmt.delimiter(",;\t|"[(b >> 2) & 3]); break;
        case 2: fmt.delimiter(",;\t|"[(b >> 2) & 3]).no_header(); break;
        default: fmt.delimiter(',').header_row((b >> 2) & 3); break;
    }
    if (b & 0x10) fmt.quote(false);
    if (b & 0x20) fmt.trim({' ', '\t'});
    fmt.variable_columns(static_cast<csv::VariableColumnPolicy>(static_cast<int>((b >> 6) & 3) - 1));
    return fmt;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > 65536) return 0;
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    try {
        auto reader = csv::parse(csv::string_view(text), pick_format(data[0]));
        const auto names = reader.get_col_names();
        for (auto& row : reader) {
            (void)row.to_json();
            (void)row.to_json_array();
            if (!names.empty()) {
                std::vector<std::string> subset{names.front(), names.back()};
                try { (void)row.to_json(subset); } catch (const std::exception&) {}
            }
            for (const auto& name : names) {
                try {
                    auto field = row[name];
                    (void)field.get<std::string>();
                    (void)field.is_num(); (void)field.is_int(); (void)field.is_float();
                    (void)field.is_str(); (void)field.is_null();
                    try { (void)field.get<int>(); } catch (const std::exception&) {}
                    try { (void)field.get<long long>(); } catch (const std::exception&) {}
                    try { (void)field.get<unsigned>(); } catch (const std::exception&) {}
                    try { (void)field.get<double>(); } catch (const std::exception&) {}
                    try { (void)field.get<float>(); } catch (const std::exception&) {}
                } catch (const std::exception&) {}
            }
            for (size_t i = 0; i < row.size(); i++) {
                try { (void)row[i].get<long double>(); } catch (const std::exception&) {}
            }
            std::vector<std::string> as_vec(row);
            (void)as_vec;
        }
    } catch (const std::exception&) {
    }
    return 0;
}
