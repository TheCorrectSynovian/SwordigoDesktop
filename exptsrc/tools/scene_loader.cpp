/* scene_loader.cpp — Parse Swordigo .scene protobuf files
 *
 * Pipeline:
 *   1. Read the entire .scene file into memory
 *   2. Iterate top-level field 1 entries (each is a scene object)
 *   3. For each object: extract name, components, transform
 *   4. Scan component data for mesh/texture references
 *   5. Compute scene bounds from object positions
 *
 * Error handling: every sub-parse is wrapped in try/catch so
 * malformed fields are skipped rather than crashing the viewer.
 */

#include "scene_loader.h"
#include "platform/protobuf_reader.h"

#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace av {

// ============================================================
// Helpers
// ============================================================

// Check if a string looks like printable ASCII (used to validate
// potential name/path strings extracted from protobuf bytes).
static bool is_printable(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

// Case-insensitive check for a suffix (e.g. ".pod", ".pvr").
static bool ends_with_ci(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    auto it  = s.rbegin();
    auto sit = suffix.rbegin();
    for (; sit != suffix.rend(); ++it, ++sit) {
        if (std::tolower(static_cast<unsigned char>(*it)) !=
            std::tolower(static_cast<unsigned char>(*sit)))
            return false;
    }
    return true;
}

// Try to find mesh/texture references buried in raw component bytes.
// Swordigo components sometimes embed asset paths as length-delimited
// strings within their own protobuf sub-messages.
static void scan_for_asset_refs(const std::string& raw,
                                std::string& out_mesh,
                                std::string& out_texture) {
    try {
        proto::Reader sub(raw);
        proto::Field  f;
        while (sub.read_field(f)) {
            if (f.wire_type != proto::WIRE_LEN) continue;
            const std::string& v = f.bytes_val;
            if (!is_printable(v)) continue;

            // .POD models
            if (ends_with_ci(v, ".pod") && out_mesh.empty()) {
                out_mesh = v;
            }
            // .pvr / .png textures
            if ((ends_with_ci(v, ".pvr") || ends_with_ci(v, ".png")) &&
                out_texture.empty()) {
                out_texture = v;
            }
        }
    } catch (...) {
        // Sub-parse failed — that's fine, raw bytes may not be valid
        // protobuf at all (e.g. a binary blob). Silently skip.
    }
}

// ============================================================
// Parse a single component from its nested protobuf bytes
// ============================================================
static SceneComponent parse_component(const std::string& bytes) {
    SceneComponent comp;
    comp.raw_data = bytes;

    try {
        proto::Reader reader(bytes);
        proto::Field  f;
        while (reader.read_field(f)) {
            switch (f.field_number) {
                case 1: // type name (string)
                    if (f.wire_type == proto::WIRE_LEN && is_printable(f.bytes_val))
                        comp.type_name = f.bytes_val;
                    break;
                case 2: // type ID (varint)
                    if (f.wire_type == proto::WIRE_VARINT)
                        comp.type_id = static_cast<int>(f.varint_val);
                    break;
                default:
                    break; // skip unknown fields
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[scene_loader] warning: component parse error: "
                  << e.what() << "\n";
    }

    return comp;
}

// ============================================================
// Parse a single scene object from its nested protobuf bytes
// ============================================================
static SceneObject parse_object(const std::string& bytes) {
    SceneObject obj;

    try {
        proto::Reader reader(bytes);
        proto::Field  f;
        while (reader.read_field(f)) {
            switch (f.field_number) {
                // Field 2: object name
                case 2:
                    if (f.wire_type == proto::WIRE_LEN && is_printable(f.bytes_val))
                        obj.name = f.bytes_val;
                    break;

                // Field 3: component (repeated nested message)
                case 3:
                    if (f.wire_type == proto::WIRE_LEN) {
                        SceneComponent comp = parse_component(f.bytes_val);
                        obj.components.push_back(std::move(comp));
                    }
                    break;

                // Fields 4-8: transform floats
                // Layout observed in Swordigo:
                //   4 = pos_x,  5 = pos_y,  6 = pos_z
                //   7 = rot_y (primary rotation around Y)
                //   8 = scale (uniform — stored once, applied to all axes)
                // Some scenes pack rotation/scale differently; we fall
                // back gracefully when fields are missing.
                case 4:
                    if (f.wire_type == proto::WIRE_I32)
                        obj.pos_x = f.float_val;
                    break;
                case 5:
                    if (f.wire_type == proto::WIRE_I32)
                        obj.pos_y = f.float_val;
                    break;
                case 6:
                    if (f.wire_type == proto::WIRE_I32)
                        obj.pos_z = f.float_val;
                    break;
                case 7:
                    if (f.wire_type == proto::WIRE_I32)
                        obj.rot_y = f.float_val;
                    break;
                case 8:
                    if (f.wire_type == proto::WIRE_I32) {
                        obj.scale_x = f.float_val;
                        obj.scale_y = f.float_val;
                        obj.scale_z = f.float_val;
                    }
                    break;

                default:
                    break; // unknown fields — skip
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[scene_loader] warning: object parse error: "
                  << e.what() << "\n";
    }

    // Post-process: scan components for asset references
    for (const auto& comp : obj.components) {
        // Tag well-known component types
        if (comp.type_name == "MeshRenderer" || comp.type_name == "SkinnedMeshRenderer") {
            scan_for_asset_refs(comp.raw_data, obj.mesh_name, obj.texture_name);
        }
        if (comp.type_name == "Background") {
            std::string dummy_tex;
            scan_for_asset_refs(comp.raw_data, obj.background_name, dummy_tex);
            if (obj.texture_name.empty() && !dummy_tex.empty())
                obj.texture_name = dummy_tex;
        }
        // Generic fallback: if we still have no mesh, scan every component
        if (obj.mesh_name.empty()) {
            scan_for_asset_refs(comp.raw_data, obj.mesh_name, obj.texture_name);
        }
    }

    return obj;
}

// ============================================================
// Compute AABB from all object positions
// ============================================================
static void compute_bounds(SceneData& scene) {
    if (scene.objects.empty()) return;

    float min_x = scene.objects[0].pos_x;
    float min_y = scene.objects[0].pos_y;
    float min_z = scene.objects[0].pos_z;
    float max_x = min_x, max_y = min_y, max_z = min_z;

    for (const auto& obj : scene.objects) {
        min_x = std::min(min_x, obj.pos_x);
        min_y = std::min(min_y, obj.pos_y);
        min_z = std::min(min_z, obj.pos_z);
        max_x = std::max(max_x, obj.pos_x);
        max_y = std::max(max_y, obj.pos_y);
        max_z = std::max(max_z, obj.pos_z);
    }

    scene.bounds_min[0] = min_x;
    scene.bounds_min[1] = min_y;
    scene.bounds_min[2] = min_z;
    scene.bounds_max[0] = max_x;
    scene.bounds_max[1] = max_y;
    scene.bounds_max[2] = max_z;
}

// ============================================================
// Public API
// ============================================================

SceneData scene_load(const std::string& path) {
    SceneData scene;

    // --- Read file -----------------------------------------------------------
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[scene_loader] error: cannot open " << path << "\n";
        return scene;
    }

    const auto size = file.tellg();
    if (size <= 0) {
        std::cerr << "[scene_loader] error: empty file " << path << "\n";
        return scene;
    }

    std::vector<uint8_t> buf(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buf.data()), size);
    file.close();

    scene.filename = fs::path(path).filename().string();

    // --- Parse top-level fields ----------------------------------------------
    try {
        proto::Reader reader(buf.data(), buf.size());
        proto::Field  f;

        while (reader.read_field(f)) {
            // Only field 1 (WIRE_LEN) carries scene objects
            if (f.field_number == 1 && f.wire_type == proto::WIRE_LEN) {
                SceneObject obj = parse_object(f.bytes_val);
                scene.objects.push_back(std::move(obj));
            }
            // Other top-level fields (scene metadata, version, etc.)
            // are currently ignored — add parsing here as the schema
            // becomes better understood.
        }
    } catch (const std::exception& e) {
        std::cerr << "[scene_loader] error: top-level parse failed: "
                  << e.what() << "\n";
        // Return whatever we managed to parse so far
    }

    scene.object_count = static_cast<int>(scene.objects.size());
    compute_bounds(scene);

    std::cout << "[scene_loader] loaded " << scene.filename
              << ": " << scene.object_count << " objects\n";

    return scene;
}

} // namespace av
