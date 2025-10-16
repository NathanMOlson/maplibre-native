#include <mbgl/renderer/render_terrain.hpp>
#include <mbgl/renderer/update_parameters.hpp>
#include <mbgl/renderer/render_source.hpp>
#include <mbgl/renderer/render_pass.hpp>
#include <mbgl/renderer/render_tree.hpp>
#include <mbgl/renderer/render_static_data.hpp>
#include <mbgl/renderer/change_request.hpp>
#include <mbgl/renderer/layer_group.hpp>
#include <mbgl/gfx/context.hpp>
#include <mbgl/gfx/drawable.hpp>
#include <mbgl/gfx/drawable_impl.hpp>
#include <mbgl/gfx/drawable_builder.hpp>
#include <mbgl/gfx/shader_registry.hpp>
#include <mbgl/gfx/color_mode.hpp>
#include <mbgl/shaders/shader_source.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/logging.hpp>

#include <cmath>
#include <cstring>

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

void RenderTerrain::update(gfx::ShaderRegistry& shaders,
                           gfx::Context& context,
                           const TransformState& /*state*/,
                           const std::shared_ptr<UpdateParameters>& /*updateParameters*/,
                           const RenderTree& /*renderTree*/,
                           UniqueChangeRequestVec& changes) {
    // Create layer group if we don't have one
    if (!layerGroup) {
        if (auto layerGroup_ = context.createLayerGroup(TERRAIN_LAYER_INDEX, /*initialCapacity=*/1, "terrain")) {
            layerGroup = std::move(layerGroup_);
            activateLayerGroup(true, changes);
            Log::Info(Event::Render, "Created terrain layer group");
        } else {
            Log::Error(Event::Render, "Failed to create terrain layer group");
            return;
        }
    }

    // If we already have a drawable, skip recreation for now
    // TODO: Update drawable when terrain properties change
    if (layerGroup && layerGroup->getDrawableCount() > 0) {
        return;
    }

    // Create the terrain drawable
    auto drawable = createDrawable(context, shaders);
    if (drawable) {
        // Cast to base LayerGroup to access the correct addDrawable overload
        if (auto* lg = static_cast<LayerGroup*>(layerGroup.get())) {
            lg->addDrawable(std::move(drawable));
            Log::Info(Event::Render, "Added terrain drawable to layer group");
        }
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
    // Store as int16_t (short) for Metal short2 attribute format
    std::vector<int16_t> vertices;
    vertices.reserve(totalVertices * 2); // 2 shorts per vertex (x, y)

    const float step = static_cast<float>(util::EXTENT) / static_cast<float>(gridSize);

    for (size_t y = 0; y < verticesPerSide; ++y) {
        for (size_t x = 0; x < verticesPerSide; ++x) {
            vertices.push_back(static_cast<int16_t>(x * step));
            vertices.push_back(static_cast<int16_t>(y * step));
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

    // Store mesh data with raw vertices and indices
    mesh = TerrainMesh{
        nullptr, // vertexBuffer - will be created when creating drawable
        nullptr, // indexBuffer - will be created when creating drawable
        vertices.size() / 2,
        indices.size(),
        std::move(vertices),
        std::move(indices)
    };

    Log::Info(Event::General,
              "Terrain mesh generated: " + std::to_string(mesh->vertexCount) + " vertices, " +
              std::to_string(mesh->indexCount) + " indices");
}

std::unique_ptr<gfx::Drawable> RenderTerrain::createDrawable(gfx::Context& context, gfx::ShaderRegistry& shaders) {
    // Ensure mesh is generated
    const auto& terrainMesh = getMesh(context);

    if (terrainMesh.vertices.empty() || terrainMesh.indices.empty()) {
        Log::Error(Event::Render, "Terrain mesh is empty, cannot create drawable");
        return nullptr;
    }

    // Get terrain shader using the same pattern as background layer
    auto terrainShader = context.getGenericShader(shaders, "TerrainShader");
    if (!terrainShader) {
        Log::Error(Event::Render, "Terrain shader not found");
        return nullptr;
    }

    // Create drawable builder
    auto builder = context.createDrawableBuilder("terrain");
    if (!builder) {
        Log::Error(Event::Render, "Failed to create drawable builder for terrain");
        return nullptr;
    }

    // Configure builder - terrain is 3D and writes depth
    builder->setShader(terrainShader);
    builder->setRenderPass(RenderPass::Pass3D);
    builder->setDepthType(gfx::DepthMaskType::ReadWrite);
    builder->setColorMode(gfx::ColorMode::unblended());
    builder->setEnableDepth(true);
    builder->setIs3D(true);

    // Set vertex data - copy vertices to raw buffer
    std::vector<uint8_t> vertexData(terrainMesh.vertices.size() * sizeof(int16_t));
    std::memcpy(vertexData.data(), terrainMesh.vertices.data(), vertexData.size());
    builder->setRawVertices(std::move(vertexData), terrainMesh.vertexCount, gfx::AttributeDataType::Short2);

    // Set index data and segments
    // Use RenderStaticData to create a single segment for the entire mesh
    std::vector<uint16_t> indexData = terrainMesh.indices;

    // Create a segment that covers the entire mesh
    const auto segs = RenderStaticData::tileTriangleSegments();
    builder->setSegments(gfx::Triangles(), std::move(indexData), segs.data(), segs.size());

    // TODO: Bind DEM texture when we have source lookup
    // builder->setTexture(demTexture, textureIndex);

    // Flush to create the drawable
    builder->flush(context);

    // Get the drawable
    auto drawables = builder->clearDrawables();
    if (drawables.empty()) {
        Log::Error(Event::Render, "Failed to create terrain drawable");
        return nullptr;
    }

    Log::Info(Event::Render, "Terrain drawable created successfully");
    return std::move(drawables[0]);
}

RenderSource* RenderTerrain::findDEMSource(const UpdateParameters& /*parameters*/) {
    // TODO: Implement source lookup
    // This would iterate through parameters.sources to find the raster-dem source
    // matching impl->sourceID
    return nullptr;
}

void RenderTerrain::activateLayerGroup(bool activate, UniqueChangeRequestVec& changes) {
    if (layerGroup) {
        if (activate) {
            changes.emplace_back(std::make_unique<AddLayerGroupRequest>(layerGroup));
        } else {
            changes.emplace_back(std::make_unique<RemoveLayerGroupRequest>(layerGroup));
        }
    }
}

} // namespace mbgl
