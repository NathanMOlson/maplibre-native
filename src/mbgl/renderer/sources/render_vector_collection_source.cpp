#include <mbgl/renderer/sources/render_vector_collection_source.hpp>

#include <mbgl/renderer/render_tile.hpp>
#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/renderer/tile_parameters.hpp>
#include <mbgl/tile/vector_mlt_tile.hpp>
#include <mbgl/tile/vector_mvt_tile.hpp>
#include <mbgl/tile/custom_geometry_tile.hpp>

namespace mbgl {

using namespace style;

RenderVectorCollectionSource::RenderVectorCollectionSource(Immutable<style::TileSource::Impl> impl_,
                                                           const TaggedScheduler& threadPool_)
    : RenderTileSource(std::move(impl_), threadPool_),
      loader([&](const CanonicalTileID&) {}, [&](const CanonicalTileID&) {}),
      loaderActor({}) {
    auto mb = std::make_shared<Mailbox>(*Scheduler::GetCurrent());
    loaderActor = ActorRef<CustomTileLoader>(loader, mb);
}

void RenderVectorCollectionSource::updateInternal(const Tileset& tileset,
                                                  const std::vector<Immutable<style::LayerProperties>>& layers,
                                                  const bool needsRendering,
                                                  const bool needsRelayout,
                                                  const TileParameters& parameters) {
    tilePyramid.update(
        layers,
        needsRendering,
        needsRelayout,
        parameters,
        *baseImpl,
        util::tileSize_I,
        tileset.zoomRange,
        tileset.bounds,
        [&](const OverscaledTileID& tileID, TileObserver* observer_) -> std::unique_ptr<VectorTile> {
            if (!isMLT.has_value()) {
                auto impl = staticImmutableCast<style::TileSource::Impl>(baseImpl);
                assert(impl->tileset); // we should have one by now
                if (impl->tileset) {
                    isMLT = (impl->tileset->vectorEncoding == Tileset::VectorEncoding::MLT);
                }
            }
            if (isMLT && *isMLT) {
                return std::make_unique<VectorMLTTile>(tileID, baseImpl->id, parameters, tileset, observer_);
            } else {
                return std::make_unique<VectorMVTTile>(tileID, baseImpl->id, parameters, tileset, observer_);
            }
        });
}

void RenderVectorCollectionSource::update(Immutable<style::Source::Impl> baseImpl_,
                                          const std::vector<Immutable<style::LayerProperties>>& layers,
                                          const bool needsRendering,
                                          const bool needsRelayout,
                                          const TileParameters& parameters) {
    std::swap(baseImpl, baseImpl_);

    enabled = needsRendering;

    const auto& implTileset = static_cast<const style::TileSource::Impl&>(*baseImpl).tileset;
    // In Continuous mode, keep the existing tiles if the new cachedTileset is
    // not yet available, thus providing smart style transitions without
    // flickering. In other modes, allow clearing the tile pyramid first, before
    // the early return in order to avoid render tests being flaky.
    bool canUpdateTileset = implTileset || parameters.mode != MapMode::Continuous;
    if (canUpdateTileset && cachedTileset != implTileset) {
        cachedTileset = implTileset;

        // TODO: this removes existing buckets, and will cause flickering.
        // Should instead refresh tile data in place.
        tilePyramid.clearAll();
    }

    if (!cachedTileset) return;

    updateInternal(*cachedTileset, layers, needsRendering, needsRelayout, parameters);

    tileParameters = std::make_unique<TileParameters>(parameters);
}

void addTileData(CustomGeometryTile& superTile, const RenderTile&) {
    mapbox::feature::feature_collection<double> features;
    features.push_back(mapbox::feature::feature<double>{mapbox::geometry::point<double>(0, 0)});
    superTile.setTileData(features);
}

RenderTiles RenderVectorCollectionSource::convertRenderTiles(const SourcePrepareParameters& parameters) const {
    CustomGeometrySource::TileOptions options{
        .tolerance = 0.375, .tileSize = util::tileSize_I, .buffer = 128, .clip = false, .wrap = false};
    Immutable<CustomGeometrySource::TileOptions> tileOptions = makeMutable<CustomGeometrySource::TileOptions>(
        std::move(options));
    CustomGeometryTile superTile(
        OverscaledTileID(0, 0, 0), baseImpl->id, *tileParameters, tileOptions, *loaderActor, nullptr);

    for (const auto& tile : *renderTiles) {
        addTileData(superTile, tile);
    }

    renderTile = std::make_unique<mbgl::RenderTile>(superTile.id.toUnwrapped(), superTile);
    renderTile->prepare(parameters);

    auto result = std::make_shared<std::vector<std::reference_wrapper<const RenderTile>>>();
    result->emplace_back(*renderTile);
    return result;
}

void RenderVectorCollectionSource::prepare(const SourcePrepareParameters& parameters) {
    RenderTileSource::prepare(parameters);
    convertedRenderTiles = convertRenderTiles(parameters);
}

RenderTiles RenderVectorCollectionSource::getRenderTiles() const {
    if (!filteredRenderTiles) {
        auto result = std::make_shared<std::vector<std::reference_wrapper<const RenderTile>>>();
        for (const auto& tile : *convertedRenderTiles) {
            result->emplace_back(tile);
        }
        filteredRenderTiles = std::move(result);
    }
    return filteredRenderTiles;
}

} // namespace mbgl
