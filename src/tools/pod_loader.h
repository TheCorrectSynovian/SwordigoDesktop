#pragma once
// pod_loader.h — PowerVR POD model parser (chunk-based tag-length-data format)
//
// Standalone reusable module. No OpenGL or ImGui dependencies.
// Parses .pod files used by Swordigo into flat vertex/index arrays
// ready for GPU upload.
//
// Usage:
//   av::PODModel model = av::pod_load("path/to/model.pod");
//   // or from memory:
//   av::PODModel model = av::pod_parse(data_ptr, data_size);

#include <string>
#include <vector>
#include <cstdint>

namespace av {

// ─── Per-mesh data ───────────────────────────────────────────────────
struct PODMesh {
    std::vector<float>    positions;   // flat xyz, 3 floats per vertex
    std::vector<float>    normals;     // flat xyz, 3 floats per vertex
    std::vector<float>    uvs;         // flat uv,  2 floats per vertex
    std::vector<uint16_t> indices;     // triangle indices (always u16)

    int num_vertices = 0;
    int num_faces    = 0;

    // Per-mesh axis-aligned bounding box
    float min_x =  1e9f, min_y =  1e9f, min_z =  1e9f;
    float max_x = -1e9f, max_y = -1e9f, max_z = -1e9f;
};

// ─── Whole-model aggregate ───────────────────────────────────────────
struct PODModel {
    std::vector<PODMesh> meshes;
    std::string          version;
    int                  num_frames = 0;

    // Bounding sphere (computed from union of all mesh AABBs)
    float center_x = 0, center_y = 0, center_z = 0;
    float radius   = 1.0f;

    // Totals across all meshes
    int total_vertices = 0;
    int total_faces    = 0;

    // Global AABB
    float min_x =  1e9f, min_y =  1e9f, min_z =  1e9f;
    float max_x = -1e9f, max_y = -1e9f, max_z = -1e9f;
};

// Load a POD model from a file path.  Returns an empty model on failure.
PODModel pod_load(const std::string& path);

// Parse a POD model from an in-memory buffer.
PODModel pod_parse(const uint8_t* data, size_t size);

} // namespace av
