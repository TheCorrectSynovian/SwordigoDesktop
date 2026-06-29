#include "game/save_editor_logic.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>

// Wire types
enum WireType {
    VARINT = 0,
    FIXED64 = 1,
    LENGTH_DELIMITED = 2,
    START_GROUP = 3,
    END_GROUP = 4,
    FIXED32 = 5
};

struct PbField {
    uint32_t num;
    uint32_t type;
    std::vector<uint8_t> data;
    std::vector<PbField> subfields; // For nested messages
};

uint64_t decode_varint(const uint8_t* data, size_t& pos, size_t max_pos) {
    uint64_t res = 0;
    uint64_t shift = 0;
    while (pos < max_pos) {
        uint8_t b = data[pos++];
        res |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return res;
}

void encode_varint(std::vector<uint8_t>& buf, uint64_t val) {
    do {
        uint8_t b = val & 0x7F;
        val >>= 7;
        if (val) b |= 0x80;
        buf.push_back(b);
    } while (val);
}

std::string decode_string(const uint8_t* data, size_t& pos, size_t max_pos) {
    uint64_t len = decode_varint(data, pos, max_pos);
    if (pos + len > max_pos) return "";
    std::string s((const char*)data + pos, (size_t)len);
    pos += (size_t)len;
    return s;
}

void encode_field_header(std::vector<uint8_t>& buf, uint32_t num, uint32_t type) {
    encode_varint(buf, (num << 3) | type);
}

void encode_string(std::vector<uint8_t>& buf, uint32_t field_num, const std::string& str) {
    encode_field_header(buf, field_num, LENGTH_DELIMITED);
    encode_varint(buf, str.length());
    buf.insert(buf.end(), str.begin(), str.end());
}

void parse_message(const uint8_t* data, size_t pos, size_t len, std::vector<PbField>& fields) {
    size_t end = pos + len;
    while (pos < end) {
        uint64_t header = decode_varint(data, pos, end);
        uint32_t num = header >> 3;
        uint32_t type = header & 0x7;
        
        PbField f;
        f.num = num;
        f.type = type;
        
        if (type == VARINT) {
            size_t start = pos;
            decode_varint(data, pos, end);
            f.data.assign(data + start, data + pos);
        } else if (type == FIXED64) {
            f.data.assign(data + pos, data + pos + 8);
            pos += 8;
        } else if (type == LENGTH_DELIMITED) {
            uint64_t l = decode_varint(data, pos, end);
            f.data.assign(data + pos, data + pos + (size_t)l);
            // Try recursive parse for likely submessages
            // Field 4 (GameState), Field 1 (CharacterState), etc.
            if (num == 4 || num == 1 || num == 2 || num == 11) {
                parse_message(data, pos, (size_t)l, f.subfields);
            }
            pos += (size_t)l;
        } else if (type == FIXED32) {
            f.data.assign(data + pos, data + pos + 4);
            pos += 4;
        }
        fields.push_back(f);
    }
}

bool load_swordigo_save(const std::string& path, SwordigoSave& out_save) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    
    out_save.raw_data.resize(size);
    if (!f.read((char*)out_save.raw_data.data(), size)) return false;
    
    std::vector<PbField> root_fields;
    parse_message(out_save.raw_data.data(), 0, (size_t)size, root_fields);
    
    for (auto& rf : root_fields) {
        if (rf.num == 2) { // level
            size_t p = 0;
            out_save.level = (int)decode_varint(rf.data.data(), p, rf.data.size());
        } else if (rf.num == 4) { // gameState
            for (auto& gf : rf.subfields) {
                if (gf.num == 1) { // charState
                    for (auto& cf : gf.subfields) {
                        size_t p = 0;
                        if (cf.num == 2) out_save.health = (int)decode_varint(cf.data.data(), p, cf.data.size());
                        else if (cf.num == 4) out_save.coins = (int)decode_varint(cf.data.data(), p, cf.data.size());
                        else if (cf.num == 5) out_save.max_health = (int)decode_varint(cf.data.data(), p, cf.data.size());
                        else if (cf.num == 11) { // item
                             for (auto& itf : cf.subfields) {
                                 if (itf.num == 1) {
                                     size_t sp = 0;
                                     out_save.items.push_back(std::string((const char*)itf.data.data(), itf.data.size()));
                                 }
                             }
                        }
                    }
                } else if (gf.num == 3) { // currentScene
                    out_save.scene = std::string((const char*)gf.data.data(), gf.data.size());
                } else if (gf.num == 4) { // spawnPoint
                    out_save.spawn = std::string((const char*)gf.data.data(), gf.data.size());
                }
            }
        }
    }
    
    return true;
}

// Simple re-serialization of the known fields.
// For now, we'll just reconstruct the whole message to ensure it's valid.
// A real production version would preserve unknown fields better.
bool save_swordigo_save(const std::string& path, const SwordigoSave& s) {
    std::vector<uint8_t> buf;
    
    // Field 1: Identifier (empty or same as original)
    encode_string(buf, 1, ""); 
    
    // Field 2: Level
    encode_field_header(buf, 2, VARINT);
    encode_varint(buf, s.level);
    
    // Field 3: Last Played Time (Double 0.0 for now)
    encode_field_header(buf, 3, FIXED64);
    uint64_t zero = 0;
    buf.insert(buf.end(), (uint8_t*)&zero, (uint8_t*)&zero + 8);
    
    // Field 4: GameState
    std::vector<uint8_t> gs_buf;
    
    // GS Field 1: CharacterState
    std::vector<uint8_t> cs_buf;
    encode_field_header(cs_buf, 2, VARINT); encode_varint(cs_buf, s.health);
    encode_field_header(cs_buf, 4, VARINT); encode_varint(cs_buf, s.coins);
    encode_field_header(cs_buf, 5, VARINT); encode_varint(cs_buf, s.max_health);
    for (auto& item : s.items) {
        std::vector<uint8_t> item_buf;
        encode_string(item_buf, 1, item);
        encode_field_header(cs_buf, 11, LENGTH_DELIMITED);
        encode_varint(cs_buf, item_buf.size());
        cs_buf.insert(cs_buf.end(), item_buf.begin(), item_buf.end());
    }
    
    encode_field_header(gs_buf, 1, LENGTH_DELIMITED);
    encode_varint(gs_buf, cs_buf.size());
    gs_buf.insert(gs_buf.end(), cs_buf.begin(), cs_buf.end());
    
    // GS Field 3: Scene
    encode_string(gs_buf, 3, s.scene);
    // GS Field 4: Spawn
    encode_string(gs_buf, 4, s.spawn);
    
    encode_field_header(buf, 4, LENGTH_DELIMITED);
    encode_varint(buf, gs_buf.size());
    buf.insert(buf.end(), gs_buf.begin(), gs_buf.end());
    
    // Field 14: UUID (could be restored from original if we kept it)
    encode_string(buf, 14, "e578c008-465e-43ab-bad6-4a56ea7ea34c"); 

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write((const char*)buf.data(), buf.size());
    return true;
}
