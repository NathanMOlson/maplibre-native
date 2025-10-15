#include <mbgl/renderer/render_terrain.hpp>
#include <mbgl/renderer/update_parameters.hpp>
#include <mbgl/renderer/render_source.hpp>
#include <mbgl/gfx/context.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/logging.hpp>

#include <cmath>

namespace mbgl {

RenderTerrain::RenderTerrain(Immutable<style::Terrain::Impl> impl_)
    : impl(std::move(impl_)) {
}

RenderTerrain::~RenderTerrain() = default;

void RenderTerrain::update(const UpdateParameters& parameters) {
    // Find the DEM source if we haven't already
    if (!demSource && !impl->sourceID.empty()) {
        // In a full implementation, we would look up the source from parameters.sources
        // and cache the RenderSource pointer
        // For now, this is a placeholder
    }
}

float RenderTerrain::getElevation(const UnwrappedTileID& /*tileID*/, float /*x*/, float /*y*/) const {
    // TODO: Implement DEM tile lookup and bilinear interpolation
    // This would:
    // 1. Find the DEM tile covering this coordinate
    // 2. Get the DEMData from the tile
    // 3. Perform bilinear interpolation to get elevation
    return 0.0f;
}

float RenderTerrain::getElevationWithExaggeration(const UnwrappedTileID& tileID, float x, float y) const {
    return getElevation(tileID, x, y) * getExaggeration();
}

float RenderTerrain::getExaggeration() const {
    return impl->exaggeration;
}

const std::string& RenderTerrain::getSourceID() const {
    return impl->sourceID;
}

bool RenderTerrain::isEnabled() const {
    return !impl->sourceID.empty();
}

const RenderTerrain::TerrainMesh& RenderTerrain::getMesh(gfx::Context& context) {
    if (!mesh) {
        generateMesh(context);
    }
    return *mesh;
}

void RenderTerrain::generateMesh(gfx::Context& context) {
    // Generate a regular grid mesh for terrain
    // This mesh will be reused for all tiles and displaced by DEM data in shaders

    const size_t gridSize = MESH_SIZE;
    const size_t verticesPerSide = gridSize + 1;
    const size_t totalVertices = verticesPerSide * verticesPerSide;

    // Vertex data: simple grid from 0 to util::EXTENT
    std::vector<float> vertices;
    vertices.reserve(totalVertices * 2); // 2 floats per vertex (x, y)

    const float step = static_cast<float>(util::EXTENT) / static_cast<float>(gridSize);

    for (size_t y = 0; y < verticesPerSide; ++y) {
        for (size_t x = 0; x < verticesPerSide; ++x) {
            vertices.push_back(x * step);
            vertices.push_back(y * step);
        }
    }

    // Index data: generate triangles for the grid
    std::vector<uint16_t> indices;
    indices.reserve(gridSize * gridSize * 6); // 2 triangles per grid cell, 3 indices per triangle

    for (size_t y = 0; y < gridSize; ++y) {
        for (size_t x = 0; x < gridSize; ++x) {
            // Calculate vertex indices for this grid cell
            uint16_t topLeft = static_cast<uint16_t>(y * verticesPerSide + x);
            uint16_t topRight = topLeft + 1;
            uint16_t bottomLeft = static_cast<uint16_t>((y + 1) * verticesPerSide + x);
            uint16_t bottomRight = bottomLeft + 1;

            // First triangle (top-left, bottom-left, top-right)
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Second triangle (top-right, bottom-left, bottom-right)
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    // Create GPU buffers
    // Note: The actual buffer creation is backend-specific (Metal/OpenGL)
    // For now, we store the mesh data structure. The actual GPU upload
    // would happen in the backend-specific rendering code.

    mesh = TerrainMesh{
        nullptr, // vertexBuffer - will be created by backend
        nullptr, // indexBuffer - will be created by backend
        vertices.size() / 2,
        indices.size()
    };

    // TODO: Actually create the GPU buffers using the context
    // This would involve calling context-specific buffer creation methods
    // For example: context.createVertexBuffer(vertices) for Metal or GL

    Log::Info(Event::General,
              "Terrain mesh generated: " + std::to_string(mesh->vertexCount) + " vertices, " +
              std::to_string(mesh->indexCount) + " indices");
}

RenderSource* RenderTerrain::findDEMSource(const UpdateParameters& /*parameters*/) {
    // TODO: Implement source lookup
    // This would iterate through parameters.sources to find the raster-dem source
    // matching impl->sourceID
    return nullptr;
}

} // namespace mbgl
