#include "platform/scl_parser.h"
#include "platform/protobuf_reader.h"
#include <fstream>
#include <iostream>

namespace scl {

static bool extract_from_program(const std::string& prog_bytes, std::string& out_lua) {
    try {
        proto::Reader r(prog_bytes);
        proto::Field f;
        while (r.read_field(f)) {
            if (f.wire_type == proto::WIRE_LEN && (f.field_number == 2 || f.field_number == 3)) {
                out_lua = f.as_string();
                return true;
            }
        }
    } catch (...) {}
    return false;
}

static bool extract_from_scl_bytes(const std::string& scl_bytes, std::string& out_lua) {
    try {
        proto::Reader r(scl_bytes);
        proto::Field f;
        while (r.read_field(f)) {
            if (f.wire_type == proto::WIRE_LEN) {
                if (f.field_number == 5) {
                    if (extract_from_program(f.as_string(), out_lua)) {
                        return true;
                    }
                } else {
                    if (extract_from_scl_bytes(f.as_string(), out_lua)) {
                        return true;
                    }
                }
            }
        }
    } catch (...) {}
    return false;
}

std::string extract_lua(const std::string& scl_filepath) {
    std::ifstream file(scl_filepath, std::ios::binary);
    if (!file) {
        std::cerr << "[SclParser] Failed to open: " << scl_filepath << std::endl;
        return "";
    }
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::string lua;
    if (extract_from_scl_bytes(bytes, lua)) {
        return lua;
    }
    return "";
}

} // namespace scl
