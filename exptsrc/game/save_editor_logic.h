#ifndef SAVE_EDITOR_LOGIC_H
#define SAVE_EDITOR_LOGIC_H

#include <vector>
#include <string>
#include <stdint.h>

struct SwordigoSave {
    int level = 1;
    int health = 100;
    int max_health = 100;
    int coins = 0;
    std::string scene = "town_herohouse";
    std::string spawn = "spawn_default";
    std::vector<std::string> items;
    std::vector<std::string> spells;
    
    // Internal fields for re-serialization (to preserve unknown fields)
    std::vector<uint8_t> raw_data;
};

bool load_swordigo_save(const std::string& path, SwordigoSave& out_save);
bool save_swordigo_save(const std::string& path, const SwordigoSave& in_save);

// Helpers for manual protobuf manipulation
uint64_t decode_varint(const uint8_t* data, size_t& pos, size_t max_pos);
void encode_varint(std::vector<uint8_t>& buf, uint64_t val);
std::string decode_string(const uint8_t* data, size_t& pos, size_t max_pos);
void encode_string(std::vector<uint8_t>& buf, uint32_t field_num, const std::string& str);
void encode_field_header(std::vector<uint8_t>& buf, uint32_t num, uint32_t type);

#endif
