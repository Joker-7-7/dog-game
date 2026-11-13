#include "model.h"
#include "collision_detector.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {

int CellIndex(double coord) noexcept {
    return static_cast<int>(std::floor(coord / kRoadHalfWidth + 1e-6));
}

bool IsClose(double a, double b) noexcept {
    return std::abs(a - b) <= 1e-9 * std::max(std::abs(a), std::abs(b));
}

Position ClampToBounds(const RoadBounds& bounds, const Position& pos) noexcept {
    return {
        std::clamp(pos.x, bounds.x_min, bounds.x_max),
        std::clamp(pos.y, bounds.y_min, bounds.y_max)
    };
}

void MoveDog(Dog& dog, const Map& map, double dt) {
    const auto speed = dog.GetSpeed();
    if (speed.vx == 0.0 && speed.vy == 0.0) {
        return;
    }

    const Position start = dog.GetPosition();
    const Position estimated{start.x + speed.vx * dt, start.y + speed.vy * dt};

    const auto roads = map.FindRoadsContaining(start);
    if (roads.empty()) {
        dog.SetSpeed({0.0, 0.0});
        return;
    }

    Position best = start;
    double best_distance = -1.0;
    for (const Road* road : roads) {
        const Position clamped = ClampToBounds(road->GetBounds(), estimated);
        const double dx = clamped.x - start.x;
        const double dy = clamped.y - start.y;
        const double distance = dx * dx + dy * dy;
        if (distance > best_distance) {
            best_distance = distance;
            best = clamped;
        }
    }

    dog.SetPosition(best);
    if (!IsClose(best.x, estimated.x) || !IsClose(best.y, estimated.y)) {
        dog.SetSpeed({0.0, 0.0});
    }
}

// Width of the dog and the item when collecting.
constexpr double kDogGatherWidth = 0.4;
constexpr double kItemGatherWidth = 0.0;

class GathererProvider : public collision_detector::ItemGathererProvider {
public:
    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

void CollectLostObjects(Map& map, std::vector<std::unique_ptr<Dog>>& dogs,
                        const std::vector<Position>& start_positions) {
    const auto& lost_objects = map.GetLostObjects();
    if (lost_objects.empty() || dogs.empty()) {
        return;
    }

    GathererProvider provider;
    provider.items_.reserve(lost_objects.size());
    for (const auto& lost_object : lost_objects) {
        provider.items_.push_back({lost_object.position, kItemGatherWidth});
    }

    provider.gatherers_.reserve(dogs.size());
    for (size_t i = 0; i < dogs.size(); ++i) {
        provider.gatherers_.push_back(
            {start_positions.at(i), dogs.at(i)->GetPosition(), kDogGatherWidth});
    }

    const auto events = collision_detector::FindGatherEvents(provider);

    std::vector<bool> collected(lost_objects.size(), false);
    for (const auto& event : events) {
        if (collected.at(event.item_id)) {
            continue;
        }

        Dog& dog = *dogs.at(event.gatherer_id);
        if (dog.GetBag().size() >= map.GetBagCapacity()) {
            continue;
        }

        const LostObject& lost_object = lost_objects.at(event.item_id);
        dog.AddToBag(BagItem{lost_object.id, lost_object.type_index});
        collected[event.item_id] = true;
    }

    map.RemoveLostObjects(collected);
}

bool SegmentIntersectsRect(const Position& from, const Position& to,
                           double x_min, double y_min, double x_max, double y_max) {
    double t_enter = 0.0;
    double t_exit = 1.0;

    const double dx = to.x - from.x;
    const double dy = to.y - from.y;

    const auto clip = [&](double origin, double direction, double lo, double hi) {
        if (direction == 0.0) {
            return origin >= lo && origin <= hi;
        }
        double t1 = (lo - origin) / direction;
        double t2 = (hi - origin) / direction;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        t_enter = std::max(t_enter, t1);
        t_exit = std::min(t_exit, t2);
        return t_enter <= t_exit;
    };

    return clip(from.x, dx, x_min, x_max) && clip(from.y, dy, y_min, y_max);
}

bool CrossedBase(const Map& map, const Position& from, const Position& to) {
    for (const Office& office : map.GetOffices()) {
        const double half_width = static_cast<double>(office.GetOffset().dx);
        const double half_height = static_cast<double>(office.GetOffset().dy);
        const Position center{static_cast<double>(office.GetPosition().x),
                              static_cast<double>(office.GetPosition().y)};

        if (SegmentIntersectsRect(from, to,
                                  center.x - half_width, center.y - half_height,
                                  center.x + half_width, center.y + half_height)) {
            return true;
        }
    }
    return false;
}

}  // namespace

RoadBounds Road::GetBounds() const noexcept {
    const double x_min = std::min(start_.x, end_.x);
    const double x_max = std::max(start_.x, end_.x);
    const double y_min = std::min(start_.y, end_.y);
    const double y_max = std::max(start_.y, end_.y);
    return {x_min - kRoadHalfWidth, y_min - kRoadHalfWidth,
            x_max + kRoadHalfWidth, y_max + kRoadHalfWidth};
}

void Map::AddRoad(const Road& road) {
    const size_t road_index = roads_.size();
    roads_.emplace_back(road);

    const RoadBounds bounds = road.GetBounds();
    const int x_first = CellIndex(bounds.x_min);
    const int x_last = CellIndex(bounds.x_max);
    const int y_first = CellIndex(bounds.y_min);
    const int y_last = CellIndex(bounds.y_max);

    for (int x = x_first; x <= x_last; ++x) {
        for (int y = y_first; y <= y_last; ++y) {
            road_cells_[CellKey{x, y}].push_back(road_index);
        }
    }
}

std::vector<const Road*> Map::FindRoadsContaining(const Position& pos) const noexcept {
    std::vector<const Road*> result;

    const auto it = road_cells_.find(CellKey{CellIndex(pos.x), CellIndex(pos.y)});
    if (it == road_cells_.end()) {
        return result;
    }

    for (const size_t index : it->second) {
        const RoadBounds bounds = roads_[index].GetBounds();
        if (pos.x >= bounds.x_min && pos.x <= bounds.x_max
            && pos.y >= bounds.y_min && pos.y <= bounds.y_max) {
            result.push_back(&roads_[index]);
        }
    }
    return result;
}

void Game::Tick(TimeDelta delta_ms) {
    const double dt = static_cast<double>(delta_ms) / 1000.0;

    for (auto& session : sessions_) {
        Map* map = session.GetMap();
        if (map == nullptr) {
            continue;
        }

        auto& dogs = session.GetDogs();

        std::vector<Position> start_positions;
        start_positions.reserve(dogs.size());
        for (auto& dog : dogs) {
            start_positions.push_back(dog->GetPosition());
            MoveDog(*dog, *map, dt);
        }

        CollectLostObjects(*map, dogs, start_positions);

        const auto* loot_types = extra_data_.FindLootTypes(map->GetId());
        for (size_t i = 0; i < dogs.size(); ++i) {
            if (CrossedBase(*map, start_positions.at(i), dogs.at(i)->GetPosition())) {
                Dog& dog = *dogs.at(i);
                int64_t score_delta = 0;
                for (const BagItem& item : dog.GetBag()) {
                    if (loot_types != nullptr
                        && (*loot_types)[item.type].value.has_value()) {
                        score_delta += *(*loot_types)[item.type].value;
                    }
                }
                dog.AddScore(score_delta);
                dog.ClearBag();
            }
        }

        map->UpdateLostObjects(std::chrono::milliseconds{delta_ms}, extra_data_, loot_generator_,
                               static_cast<unsigned>(session.GetDogs().size()));
    }

    // Periodically save state based on game time.
    if (state_save_callback_) {
        time_since_save_ms_ += delta_ms;
        if (state_save_period_ms_ > 0 && time_since_save_ms_ >= state_save_period_ms_) {
            time_since_save_ms_ = 0;
            state_save_callback_();
        }
    }
}

void GameSession::AddDog(std::unique_ptr<Dog> dog) {
    dogs_.emplace_back(std::move(dog));
}

GameSession* Game::FindSession(const Map::Id& map_id) {
    for (auto& session : sessions_) {
        if (session.GetMap() && session.GetMap()->GetId() == map_id) {
            return &session;
        }
    }
    return nullptr;
}

GameSession& Game::AddSession(Map* map) {
    return sessions_.emplace_back(map);
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Map::UpdateLostObjects(std::chrono::milliseconds time_delta, const ExtraData& extra_data,
                            loot_gen::LootGenerator& loot_generator, unsigned looters) {
    const auto* loot_types = extra_data.FindLootTypes(id_);
    if (loot_types == nullptr || loot_types->empty() || roads_.empty()) {
        return;
    }

    const unsigned generated_loot = loot_generator.Generate(
        time_delta, static_cast<unsigned>(lost_objects_.size()), looters);
    if (generated_loot == 0) {
        return;
    }

    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<size_t> type_dist(0, loot_types->size() - 1);

    for (unsigned i = 0; i < generated_loot; ++i) {
        lost_objects_.push_back({
            next_lost_object_id_++,
            type_dist(gen),
            utils::GetSpawnPoint(*this, true)
        });
    }
}

void Map::RemoveLostObjects(const std::vector<bool>& collected) {
    size_t kept = 0;
    for (size_t i = 0; i < lost_objects_.size(); ++i) {
        if (!collected.at(i)) {
            lost_objects_[kept++] = lost_objects_[i];
        }
    }
    lost_objects_.resize(kept);
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

}
