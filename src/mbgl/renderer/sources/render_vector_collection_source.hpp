#pragma once

#include <mbgl/renderer/sources/render_tile_source.hpp>
#include <mbgl/renderer/tile_pyramid.hpp>
#include <mbgl/style/sources/tile_source_impl.hpp>

namespace mbgl {

class RenderVectorCollectionSource final : public RenderTileSource {
public:
    explicit RenderVectorCollectionSource(Immutable<style::TileSource::Impl>, const TaggedScheduler&);

private:
    void updateInternal(const Tileset&,
                        const std::vector<Immutable<style::LayerProperties>>&,
                        bool needsRendering,
                        bool needsRelayout,
                        const TileParameters&);

private:
    void update(Immutable<style::Source::Impl>,
                const std::vector<Immutable<style::LayerProperties>>&,
                bool needsRendering,
                bool needsRelayout,
                const TileParameters&) final;
    std::optional<bool> isMLT;
    std::optional<Tileset> cachedTileset;
};

} // namespace mbgl
