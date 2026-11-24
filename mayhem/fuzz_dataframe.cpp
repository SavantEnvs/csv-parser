// CSVReader (stream) -> csv::DataFrame (unkeyed and keyed with a fuzz-chosen duplicate
// key policy) and csv::try_csv_data_types type inference. Non-throwing try_* API.
#include <stdint.h>
#include <stddef.h>
#include <sstream>
#include <string>
#include "csv.hpp"

static csv::CSVFormat pick_format(uint8_t b) {
    csv::CSVFormat fmt;
    csv::CSVError error;
    if (b & 1) (void)fmt.try_delimiter(std::vector<char>{',', ';', '\t', '|'}, error);
    else (void)fmt.try_delimiter(",;\t|"[(b >> 1) & 3], error);
    if (b & 8) fmt.no_header();
    fmt.variable_columns(static_cast<csv::VariableColumnPolicy>(static_cast<int>((b >> 4) & 3) - 1));
    fmt.threading((b & 0x40) != 0);
    return fmt;
}

static void walk_frame(const csv::DataFrame<>& frame) {
    (void)frame.n_rows();
    const auto& cols = frame.columns();
    for (size_t i = 0; i < frame.n_rows(); i++) {
        auto row = frame.at(i);
        csv::CSVError error;
        for (size_t c = 0; c < row.size(); c++) {
            auto cell = row.try_field(c, error);
            (void)cell.get_sv();
        }
        (void)row.try_to_json({}, error);
    }
    for (const auto& name : cols) {
        csv::CSVError error;
        (void)frame.try_column<std::string>(name, error);
        (void)frame.try_column<double>(name, error);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2 || size > 65536) return 0;
    const auto fmt = pick_format(data[0]);
    const uint8_t mode = data[1];
    const std::string text(reinterpret_cast<const char*>(data + 2), size - 2);

    {
        std::istringstream in(text);
        csv::CSVError error;
        auto reader = csv::CSVReader::try_open(in, error, fmt);
        if (reader) {
            auto frame = csv::DataFrame<>::try_create(*reader, error);
            if (frame) walk_frame(*frame);
        }
    }
    {
        std::istringstream in(text);
        csv::CSVError error;
        auto reader = csv::CSVReader::try_open(in, error, fmt);
        if (reader) {
            auto policy = static_cast<csv::DataFrameOptions::DuplicateKeyPolicy>(mode % 3);
            auto frame = csv::DataFrame<std::string>::try_create(*reader,
                [](const csv::CSVRow& row) {
                    return row.size() ? std::string(row[0].get_sv()) : std::string();
                }, error, policy);
            if (frame) {
                for (size_t i = 0; i < frame->n_rows(); i++) (void)frame->at(i).key();
            }
        }
    }
    {
        std::istringstream in(text);
        csv::CSVError error;
        auto reader = csv::CSVReader::try_open(in, error, fmt);
        if (reader) (void)csv::try_csv_data_types(*reader, error);
    }
    return 0;
}
