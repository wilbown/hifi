//
//  RenderFetchCullSortTask.cpp
//  render/src/
//
//  Created by Zach Pomerantz on 12/22/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderFetchCullSortTask.h"

#include "CullTask.h"
#include "FilterTask.h"
#include "SortTask.h"

using namespace render;

void RenderFetchCullSortTask::build(JobModel& task, const Varying& input, Varying& output, CullFunctor cullFunctor, uint8_t tagBits, uint8_t tagMask) {
    cullFunctor = cullFunctor ? cullFunctor : [](const RenderArgs*, const AABox&){ return true; };

    // CPU jobs:
    // Fetch and cull the items from the scene
    const ItemFilter filter = ItemFilter::Builder::visibleWorldItems().withoutLayered().withTagBits(tagBits, tagMask);
    const auto spatialFilter = render::Varying(filter);
    const auto fetchInput = FetchSpatialTree::Inputs(filter, glm::ivec2(0,0)).asVarying();
    const auto spatialSelection = task.addJob<FetchSpatialTree>("FetchSceneSelection", fetchInput);
    const auto cullInputs = CullSpatialSelection::Inputs(spatialSelection, spatialFilter).asVarying();
    const auto culledSpatialSelection = task.addJob<CullSpatialSelection>("CullSceneSelection", cullInputs, cullFunctor, RenderDetails::ITEM);

    // Overlays are not culled
    const ItemFilter overlayfilter = ItemFilter::Builder().withVisible().withoutSubMetaCulled().withTagBits(tagBits, tagMask);
    const auto nonspatialFilter = render::Varying(overlayfilter);
    const auto nonspatialSelection = task.addJob<FetchNonspatialItems>("FetchOverlaySelection", nonspatialFilter);

    // Multi filter visible items into different buckets
    const int NUM_SPATIAL_FILTERS = 4; 
    const int NUM_NON_SPATIAL_FILTERS = 3;
    const int OPAQUE_SHAPE_BUCKET = 0;
    const int TRANSPARENT_SHAPE_BUCKET = 1;
    const int LIGHT_BUCKET = 2;
    const int META_BUCKET = 3;
    const int BACKGROUND_BUCKET = 2;
    MultiFilterItems<NUM_SPATIAL_FILTERS>::ItemFilterArray spatialFilters = { {
            ItemFilter::Builder::opaqueShape(),
            ItemFilter::Builder::transparentShape(),
            ItemFilter::Builder::light(),
            ItemFilter::Builder::meta()
        } };
    MultiFilterItems<NUM_NON_SPATIAL_FILTERS>::ItemFilterArray nonspatialFilters = { {
            ItemFilter::Builder::opaqueShape(),
            ItemFilter::Builder::transparentShape(),
            ItemFilter::Builder::background()
        } };
    const auto filteredSpatialBuckets = 
        task.addJob<MultiFilterItems<NUM_SPATIAL_FILTERS>>("FilterSceneSelection", culledSpatialSelection, spatialFilters)
            .get<MultiFilterItems<NUM_SPATIAL_FILTERS>::ItemBoundsArray>();
    const auto filteredNonspatialBuckets = 
       task.addJob<MultiFilterItems<NUM_NON_SPATIAL_FILTERS>>("FilterOverlaySelection", nonspatialSelection, nonspatialFilters)
            .get<MultiFilterItems<NUM_NON_SPATIAL_FILTERS>::ItemBoundsArray>();

    // Extract opaques / transparents / lights / overlays
    const auto opaques = task.addJob<DepthSortItems>("DepthSortOpaque", filteredSpatialBuckets[OPAQUE_SHAPE_BUCKET]);
    const auto transparents = task.addJob<DepthSortItems>("DepthSortTransparent", filteredSpatialBuckets[TRANSPARENT_SHAPE_BUCKET], DepthSortItems(false));
    const auto lights = filteredSpatialBuckets[LIGHT_BUCKET];
    const auto metas = filteredSpatialBuckets[META_BUCKET];

    const auto overlayOpaques = task.addJob<DepthSortItems>("DepthSortOverlayOpaque", filteredNonspatialBuckets[OPAQUE_SHAPE_BUCKET]);
    const auto overlayTransparents = task.addJob<DepthSortItems>("DepthSortOverlayTransparent", filteredNonspatialBuckets[TRANSPARENT_SHAPE_BUCKET], DepthSortItems(false));
    const auto background = filteredNonspatialBuckets[BACKGROUND_BUCKET];

    // split up the overlays into 3D front, hud
    const auto filteredOverlaysOpaque = task.addJob<FilterLayeredItems>("FilterOverlaysLayeredOpaque", overlayOpaques, ItemKey::Layer::LAYER_1);
    const auto filteredOverlaysTransparent = task.addJob<FilterLayeredItems>("FilterOverlaysLayeredTransparent", overlayTransparents, ItemKey::Layer::LAYER_1);


    output = Output(BucketList{ opaques, transparents, lights, metas, overlayOpaques, overlayTransparents,
                    filteredOverlaysOpaque.getN<FilterLayeredItems::Outputs>(0), filteredOverlaysTransparent.getN<FilterLayeredItems::Outputs>(0),
                    filteredOverlaysOpaque.getN<FilterLayeredItems::Outputs>(1), filteredOverlaysTransparent.getN<FilterLayeredItems::Outputs>(1),
                    background }, spatialSelection);
}
