#pragma once
#include <string>

namespace scl {
    // Extracts the Lua script (source or compiled bytecode) from a .scl protobuf file.
    // Returns empty string on failure.
    std::string extract_lua(const std::string& scl_filepath);
}
