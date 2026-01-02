#include <mbgl/renderer/texture_pool.hpp>
#include <mbgl/gfx/context.hpp>

namespace mbgl {
TexturePool::TexturePool(uint32_t tilesize)
    : tileSize(tilesize) {}

TexturePool::~TexturePool() {}

void TexturePool::createRenderTarget(gfx::Context& context, const UnwrappedTileID& /*id*/) {
    renderTarget = context.createRenderTarget({tileSize, tileSize}, gfx::TextureChannelDataType::UnsignedByte);
}

std::shared_ptr<RenderTarget> TexturePool::getRenderTarget(const UnwrappedTileID& /*id*/) const {
    return renderTarget;
}
} // namespace mbgl
