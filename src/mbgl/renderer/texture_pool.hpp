#pragma once

#include <mbgl/tile/tile_id.hpp>
#include <mbgl/renderer/render_target.hpp>

namespace mbgl {
class TexturePool {
public:
    TexturePool(uint32_t tilesize);
    ~TexturePool();

    std::shared_ptr<RenderTarget> getRenderTarget(const UnwrappedTileID& id) const;
    void createRenderTarget(gfx::Context& context, const UnwrappedTileID& id);

private:
    uint32_t tileSize;

    std::shared_ptr<RenderTarget> renderTarget;
};

} // namespace mbgl
