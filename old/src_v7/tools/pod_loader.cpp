// pod_loader.cpp — PowerVR POD model parser implementation
//
// POD binary format overview:
//   Each chunk:  uint32_t tag | uint32_t length | [length bytes of data]
//   Container blocks have length == 0 and are closed by a chunk whose
//   tag == (open_tag | 0x80000000).
//
// Tag ranges used by Swordigo PODs:
//   1000        Version string
//   2000-2099   Scene-level metadata
//   2012        Mesh container (opens a mesh block)
//   6000-6099   Mesh-level metadata
//   6006/6014   Interleaved vertex data container
//   6007        Face index container
//   6008        Vertex position container
//   6009        Vertex normal container
//   9000-9003   Data-element descriptors (Type/N/Stride/Payload)
//
// Interleaved vertex layout (Swordigo):
//   float3 position  (12 bytes)
//   float3 normal    (12 bytes)
//   float2 uv        ( 8 bytes)
//   ─────────────────────────
//   total stride = 32 bytes per vertex

#include "pod_loader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace av {

// ─── POD tag constants ───────────────────────────────────────────────

// Container close-bit (OR'd with open tag to mark end-of-container)
static constexpr uint32_t kEndTagBit = 0x80000000u;

// Scene-level tags
static constexpr uint32_t kTagVersion   = 1000;
static constexpr uint32_t kTagNumMesh   = 2004;
static constexpr uint32_t kTagNumFrame  = 2009;
static constexpr uint32_t kTagMesh      = 2012;  // container

// Mesh-level tags
static constexpr uint32_t kTagNumVerts  = 6000;
static constexpr uint32_t kTagNumFaces  = 6001;
static constexpr uint32_t kTagInterleaved    = 6006;  // container
static constexpr uint32_t kTagInterleavedAlt = 6014;  // alternate interleaved container
static constexpr uint32_t kTagFaces     = 6007;  // container
static constexpr uint32_t kTagVertices  = 6008;  // container
static constexpr uint32_t kTagNormals   = 6009;  // container

// Data-element tags (inside face/vertex/normal/interleaved containers)
static constexpr uint32_t kTagDataType    = 9000;
static constexpr uint32_t kTagDataN       = 9001;
static constexpr uint32_t kTagDataStride  = 9002;
static constexpr uint32_t kTagDataPayload = 9003;

// ─── Helpers ─────────────────────────────────────────────────────────

// Read a little-endian uint32 from the buffer at offset.  Returns 0
// and sets offset past end if out of bounds.
static uint32_t read_u32(const uint8_t* data, size_t size, size_t& off) {
    if (off + 4 > size) { off = size; return 0; }
    uint32_t v;
    std::memcpy(&v, data + off, 4);
    off += 4;
    return v;
}

static uint16_t read_u16(const uint8_t* data, size_t size, size_t& off) {
    if (off + 2 > size) { off = size; return 0; }
    uint16_t v;
    std::memcpy(&v, data + off, 2);
    off += 2;
    return v;
}

// ─── Data-element accumulator ────────────────────────────────────────
// Used while parsing a 6007/6008/6009/6006 container's child chunks.
struct DataElement {
    uint32_t type   = 0;   // 9000: data type enum
    uint32_t n      = 0;   // 9001: components per element
    uint32_t stride = 0;   // 9002: byte stride between elements
    const uint8_t* payload = nullptr;
    size_t payload_size    = 0;
};

// ─── Mesh parser state ───────────────────────────────────────────────

struct MeshState {
    int num_vertices = 0;
    int num_faces    = 0;

    // Non-interleaved components
    DataElement face_data;
    DataElement vert_data;
    DataElement norm_data;

    // Interleaved
    DataElement interleaved_data;
    bool has_interleaved = false;
};

// Parse a data-element container (faces / vertices / normals / interleaved).
// Reads child chunks until the matching end-tag.
static void parse_data_container(const uint8_t* data, size_t size,
                                 size_t& off, uint32_t end_tag,
                                 DataElement& out)
{
    while (off < size) {
        uint32_t tag = read_u32(data, size, off);
        uint32_t len = read_u32(data, size, off);

        if (tag == end_tag) return;  // closing tag

        switch (tag) {
            case kTagDataType:
                if (len >= 4) out.type = read_u32(data, size, off);
                else off += len;
                break;
            case kTagDataN:
                if (len >= 4) out.n = read_u32(data, size, off);
                else off += len;
                break;
            case kTagDataStride:
                if (len >= 4) out.stride = read_u32(data, size, off);
                else off += len;
                break;
            case kTagDataPayload:
                out.payload      = data + off;
                out.payload_size = len;
                off += len;
                break;
            default:
                off += len;  // skip unknown
                break;
        }
    }
}

// Extract indices from a DataElement into a PODMesh.
// Handles uint16 (type==0 or stride==2) and uint32 (type==1 or stride==4).
static void extract_indices(const DataElement& de, int num_faces, PODMesh& mesh) {
    if (!de.payload || de.payload_size == 0) return;

    const int index_count = num_faces * 3;
    mesh.indices.reserve(static_cast<size_t>(index_count));

    // Determine index width: use stride if set, else fall back to type field.
    // type 0 = uint16, type 1 = uint32 (common PowerVR convention).
    bool is_u32 = false;
    if (de.stride == 4)      is_u32 = true;
    else if (de.stride == 2) is_u32 = false;
    else if (de.type == 1)   is_u32 = true;

    if (is_u32) {
        const size_t effective_stride = (de.stride > 0) ? de.stride : 4u;
        for (int i = 0; i < index_count; ++i) {
            size_t byte_off = static_cast<size_t>(i) * effective_stride;
            if (byte_off + 4 > de.payload_size) break;
            uint32_t idx;
            std::memcpy(&idx, de.payload + byte_off, 4);
            mesh.indices.push_back(static_cast<uint16_t>(idx));
        }
    } else {
        const size_t effective_stride = (de.stride > 0) ? de.stride : 2u;
        for (int i = 0; i < index_count; ++i) {
            size_t byte_off = static_cast<size_t>(i) * effective_stride;
            if (byte_off + 2 > de.payload_size) break;
            uint16_t idx;
            std::memcpy(&idx, de.payload + byte_off, 2);
            mesh.indices.push_back(idx);
        }
    }
}

// Extract float components from a non-interleaved DataElement.
static void extract_floats(const DataElement& de, int num_verts,
                           int components, std::vector<float>& out)
{
    if (!de.payload || de.payload_size == 0) return;

    const int n = (de.n > 0) ? static_cast<int>(de.n) : components;
    const size_t effective_stride =
        (de.stride > 0) ? de.stride : static_cast<size_t>(n) * sizeof(float);

    out.reserve(static_cast<size_t>(num_verts) * static_cast<size_t>(components));

    for (int i = 0; i < num_verts; ++i) {
        size_t byte_off = static_cast<size_t>(i) * effective_stride;
        for (int c = 0; c < components; ++c) {
            size_t elem_off = byte_off + static_cast<size_t>(c) * sizeof(float);
            if (elem_off + sizeof(float) > de.payload_size) {
                out.push_back(0.0f);
                continue;
            }
            float v;
            std::memcpy(&v, de.payload + elem_off, sizeof(float));
            out.push_back(v);
        }
    }
}

// Deinterleave Swordigo's 32-byte interleaved vertex layout into
// separate position / normal / UV arrays.
static void deinterleave(const DataElement& de, int num_verts, PODMesh& mesh) {
    if (!de.payload || de.payload_size == 0) return;

    // Determine stride — Swordigo uses 32 bytes (pos3 + norm3 + uv2).
    const size_t stride = (de.stride > 0) ? de.stride : 32u;

    mesh.positions.reserve(static_cast<size_t>(num_verts) * 3);
    mesh.normals.reserve(static_cast<size_t>(num_verts) * 3);
    mesh.uvs.reserve(static_cast<size_t>(num_verts) * 2);

    for (int i = 0; i < num_verts; ++i) {
        size_t base = static_cast<size_t>(i) * stride;

        // Position: offset 0, 3 floats
        for (int c = 0; c < 3; ++c) {
            size_t off = base + static_cast<size_t>(c) * sizeof(float);
            float v = 0.0f;
            if (off + sizeof(float) <= de.payload_size)
                std::memcpy(&v, de.payload + off, sizeof(float));
            mesh.positions.push_back(v);
        }

        // Normal: offset 12, 3 floats
        for (int c = 0; c < 3; ++c) {
            size_t off = base + 12 + static_cast<size_t>(c) * sizeof(float);
            float v = 0.0f;
            if (off + sizeof(float) <= de.payload_size)
                std::memcpy(&v, de.payload + off, sizeof(float));
            mesh.normals.push_back(v);
        }

        // UV: offset 24, 2 floats
        for (int c = 0; c < 2; ++c) {
            size_t off = base + 24 + static_cast<size_t>(c) * sizeof(float);
            float v = 0.0f;
            if (off + sizeof(float) <= de.payload_size)
                std::memcpy(&v, de.payload + off, sizeof(float));
            mesh.uvs.push_back(v);
        }
    }
}

// Compute axis-aligned bounding box for a single mesh.
static void compute_mesh_aabb(PODMesh& mesh) {
    const size_t n = mesh.positions.size() / 3;
    for (size_t i = 0; i < n; ++i) {
        float x = mesh.positions[i * 3 + 0];
        float y = mesh.positions[i * 3 + 1];
        float z = mesh.positions[i * 3 + 2];
        mesh.min_x = std::min(mesh.min_x, x);
        mesh.min_y = std::min(mesh.min_y, y);
        mesh.min_z = std::min(mesh.min_z, z);
        mesh.max_x = std::max(mesh.max_x, x);
        mesh.max_y = std::max(mesh.max_y, y);
        mesh.max_z = std::max(mesh.max_z, z);
    }
}

// ─── Mesh container parser ──────────────────────────────────────────

static PODMesh parse_mesh(const uint8_t* data, size_t size, size_t& off) {
    MeshState ms;
    const uint32_t mesh_end_tag = kTagMesh | kEndTagBit;

    while (off < size) {
        uint32_t tag = read_u32(data, size, off);
        uint32_t len = read_u32(data, size, off);

        if (tag == mesh_end_tag) break;  // end of mesh container

        switch (tag) {
            case kTagNumVerts:
                if (len >= 4) ms.num_vertices = static_cast<int>(read_u32(data, size, off));
                else off += len;
                break;

            case kTagNumFaces:
                if (len >= 4) ms.num_faces = static_cast<int>(read_u32(data, size, off));
                else off += len;
                break;

            case kTagFaces:  // container, len == 0
                parse_data_container(data, size, off,
                                     kTagFaces | kEndTagBit, ms.face_data);
                break;

            case kTagVertices:  // container
                parse_data_container(data, size, off,
                                     kTagVertices | kEndTagBit, ms.vert_data);
                break;

            case kTagNormals:  // container
                parse_data_container(data, size, off,
                                     kTagNormals | kEndTagBit, ms.norm_data);
                break;

            case kTagInterleaved:     // container
            case kTagInterleavedAlt:  // alternate tag for interleaved data
                parse_data_container(data, size, off,
                                     tag | kEndTagBit, ms.interleaved_data);
                ms.has_interleaved = true;
                break;

            default:
                // Unknown leaf — skip its payload
                off += len;
                break;
        }
    }

    // ── Build PODMesh from accumulated state ────────────────────────
    PODMesh mesh;
    mesh.num_vertices = ms.num_vertices;
    mesh.num_faces    = ms.num_faces;

    // Indices
    extract_indices(ms.face_data, ms.num_faces, mesh);

    // Vertex attributes
    if (ms.has_interleaved && ms.interleaved_data.payload) {
        deinterleave(ms.interleaved_data, ms.num_vertices, mesh);
    } else {
        // Non-interleaved: separate position / normal containers
        extract_floats(ms.vert_data, ms.num_vertices, 3, mesh.positions);
        extract_floats(ms.norm_data, ms.num_vertices, 3, mesh.normals);
    }

    // Per-mesh AABB
    if (!mesh.positions.empty()) {
        compute_mesh_aabb(mesh);
    }

    return mesh;
}

// ─── Top-level parser ────────────────────────────────────────────────

PODModel pod_parse(const uint8_t* data, size_t size) {
    PODModel model;
    size_t off = 0;

    while (off < size) {
        uint32_t tag = read_u32(data, size, off);
        uint32_t len = read_u32(data, size, off);

        // Check for end-tag bits — skip them at the top level
        if (tag & kEndTagBit) {
            off += len;
            continue;
        }

        switch (tag) {
            case kTagVersion:
                if (len > 0 && off + len <= size) {
                    // Version is a null-terminated string
                    model.version.assign(
                        reinterpret_cast<const char*>(data + off),
                        strnlen(reinterpret_cast<const char*>(data + off), len));
                }
                off += len;
                break;

            case kTagNumMesh:
                if (len >= 4) {
                    uint32_t nm = read_u32(data, size, off);
                    model.meshes.reserve(nm);
                }
                off += (len > 4 ? len - 4 : 0);
                break;

            case kTagNumFrame:
                if (len >= 4) {
                    model.num_frames = static_cast<int>(read_u32(data, size, off));
                }
                off += (len > 4 ? len - 4 : 0);
                break;

            case kTagMesh:
                // Container with len == 0 — parse until matching end-tag
                model.meshes.push_back(parse_mesh(data, size, off));
                break;

            default:
                off += len;
                break;
        }
    }

    // ── Aggregate stats and bounding geometry ───────────────────────

    for (const auto& m : model.meshes) {
        model.total_vertices += m.num_vertices;
        model.total_faces    += m.num_faces;

        if (!m.positions.empty()) {
            model.min_x = std::min(model.min_x, m.min_x);
            model.min_y = std::min(model.min_y, m.min_y);
            model.min_z = std::min(model.min_z, m.min_z);
            model.max_x = std::max(model.max_x, m.max_x);
            model.max_y = std::max(model.max_y, m.max_y);
            model.max_z = std::max(model.max_z, m.max_z);
        }
    }

    // Bounding sphere: center of AABB, radius = half-diagonal
    if (model.total_vertices > 0) {
        model.center_x = (model.min_x + model.max_x) * 0.5f;
        model.center_y = (model.min_y + model.max_y) * 0.5f;
        model.center_z = (model.min_z + model.max_z) * 0.5f;

        float dx = model.max_x - model.min_x;
        float dy = model.max_y - model.min_y;
        float dz = model.max_z - model.min_z;
        model.radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;

        // Guard against degenerate (zero-size) models
        if (model.radius < 1e-6f) model.radius = 1.0f;
    }

    return model;
}

// ─── File loader ─────────────────────────────────────────────────────

PODModel pod_load(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};

    auto fsize = f.tellg();
    if (fsize <= 0) return {};

    std::vector<uint8_t> buf(static_cast<size_t>(fsize));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(buf.data()), fsize);

    if (!f) return {};  // partial read

    return pod_parse(buf.data(), buf.size());
}

} // namespace av
