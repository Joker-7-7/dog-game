#pragma once

#include "geometry.h"

#include <algorithm>
#include <vector>

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    // Squared distance to the point
    double sq_distance;
    // Fraction of the segment traversed
    double proj_ratio;
};

// Move from point a to point b and try to collect point c
CollectionResult TryCollectPoint(model::Position a, model::Position b, model::Position c);

struct Item {
    model::Position position;
    double width;
};

struct Gatherer {
    model::Position start_pos;
    model::Position end_pos;
    double width;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
};

struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector