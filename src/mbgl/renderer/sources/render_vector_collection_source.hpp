#pragma once

#include <mbgl/renderer/sources/render_tile_source.hpp>
#include <mbgl/renderer/tile_pyramid.hpp>
#include <mbgl/style/sources/tile_source_impl.hpp>
#include <mbgl/actor/actor_ref.hpp>
#include <mbgl/style/custom_tile_loader.hpp>

namespace mbgl {

class RenderVectorCollectionSource final : public RenderTileSource {
public:
    explicit RenderVectorCollectionSource(Immutable<style::TileSource::Impl>, const TaggedScheduler&);
    void prepare(const SourcePrepareParameters&) override;

private:
    void updateInternal(const Tileset&,
                        const std::vector<Immutable<style::LayerProperties>>&,
                        bool needsRendering,
                        bool needsRelayout,
                        const TileParameters&);
    RenderTiles getRenderTiles() const override;

private:
    void update(Immutable<style::Source::Impl>,
                const std::vector<Immutable<style::LayerProperties>>&,
                bool needsRendering,
                bool needsRelayout,
                const TileParameters&) final;
    RenderTiles convertRenderTiles(const SourcePrepareParameters& parameters) const;
    std::optional<bool> isMLT;
    std::optional<Tileset> cachedTileset;
    mutable RenderTiles convertedRenderTiles;
    std::unique_ptr<TileParameters> tileParameters;
    style::CustomTileLoader loader;
    std::optional<ActorRef<style::CustomTileLoader>> loaderActor;
    mutable std::unique_ptr<mbgl::RenderTile> renderTile;
};

} // namespace mbgl
