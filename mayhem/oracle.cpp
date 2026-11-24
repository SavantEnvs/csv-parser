// Known-answer functional checks for csv-parser (reader, dialect guessing, typed fields,
// JSON, writer, DataFrame, file info). Prints "PASS <name>" / "FAIL <name>" per check;
// mayhem/test.sh counts them. Built with normal flags (no sanitizers).
#include "csv.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int failures = 0;
static void check(const char* name, bool ok) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) failures++;
}

int main(int argc, char** argv) {
    using namespace csv;
    {   // header + quoted fields with embedded delimiter, quote and newline
        auto r = parse("a,b,c\n1,\"x,y\",\"he said \"\"hi\"\"\"\n2,\"multi\nline\",3\n");
        std::vector<std::vector<std::string>> rows;
        for (auto& row : r) rows.emplace_back(std::vector<std::string>(row));
        check("col_names", r.get_col_names() == std::vector<std::string>{"a", "b", "c"});
        check("row_count", rows.size() == 2);
        check("quoted_delim", rows.size() == 2 && rows[0][1] == "x,y");
        check("escaped_quote", rows.size() == 2 && rows[0][2] == "he said \"hi\"");
        check("embedded_newline", rows.size() == 2 && rows[1][1] == "multi\nline");
    }
    {   // delimiter guessing
        CSVReader r = parse("name;age;score\nann;31;4.5\nbob;42;3.25\n");
        check("guess_semicolon", r.get_format().get_delim() == ';');
        std::vector<CSVRow> rows(r.begin(), r.end());
        check("typed_int", rows.size() == 2 && rows[0]["age"].get<int>() == 31);
        check("typed_double", rows.size() == 2 && rows[1]["score"].get<double>() == 3.25);
        check("is_int", rows.size() == 2 && rows[0]["age"].is_int() && !rows[0]["name"].is_num());
        check("to_json", rows.size() == 2 &&
              rows[0].to_json() == "{\"name\":\"ann\",\"age\":31,\"score\":4.5}");
        check("to_json_array", rows.size() == 2 &&
              rows[1].to_json_array() == "[\"bob\",42,3.25]");
    }
    {   // no header
        auto r = parse_no_header("1,2\n3,4\n");
        std::vector<std::vector<std::string>> rows;
        for (auto& row : r) rows.emplace_back(std::vector<std::string>(row));
        check("no_header", rows == std::vector<std::vector<std::string>>{{"1", "2"}, {"3", "4"}});
    }
    {   // writer quoting
        std::stringstream ss;
        auto w = make_csv_writer(ss);
        w << std::vector<std::string>{"plain", "with,comma", "with\"quote"};
        check("writer_quoting", ss.str() == "plain,\"with,comma\",\"with\"\"quote\"\n");
    }
    {   // DataFrame
        std::istringstream in("k,v\nx,1\ny,2\nx,3\n");
        CSVReader reader(in);
        DataFrame<std::string> df(reader, "k");
        check("dataframe_keyed", df.n_rows() == 2 && df["x"]["v"].get<int>() == 3);
    }
    {   // data type inference
        std::istringstream in("i,f,s\n1,2.5,a\n7,3.0,b\n");
        CSVReader reader(in);
        auto types = csv_data_types(reader);
        check("data_types", types["i"] == DataType::CSV_INT8 && types["f"] == DataType::CSV_DOUBLE &&
                            types["s"] == DataType::CSV_STRING);
    }
    if (argc > 1) {   // file path (mmap) + get_file_info, as csv_info prints it
        auto info = get_file_info(argv[1]);
        check("file_info", info.n_rows == 3 && info.n_cols == 3 && info.delim == ',' &&
                           info.col_names == std::vector<std::string>{"id", "name", "value"});
    }
    return failures ? 1 : 0;
}
