#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "tagged.h"
#include "geometry.h"
#include "loot_generator.h"

namespace model {

inline constexpr double kRoadHalfWidth = 0.4;

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

    RoadBounds GetBounds() const noexcept;

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

struct LootType {
    std::string name;
    std::string file;
    std::string type;
    std::optional<int> rotation;
    std::optional<std::string> color;
    std::optional<int64_t> value;
    double scale = 0.0;
};

struct LostObject {
    size_t id = 0;
    size_t type_index = 0;
    Position position;
};

struct BagItem {
    size_t id = 0;
    size_t type = 0;
};

class ExtraData;

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;
    using LostObjects = std::vector<LostObject>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    const LostObjects& GetLostObjects() const noexcept {
        return lost_objects_;
    }

    void AddRoad(const Road& road);

    std::vector<const Road*> FindRoadsContaining(const Position& pos) const noexcept;

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    void AddLostObject(const LostObject& lost_object) {
        lost_objects_.emplace_back(lost_object);
    }

    void RemoveLostObjects(const std::vector<bool>& collected);

    size_t GetNextLostObjectId() const noexcept {
        return next_lost_object_id_;
    }

    void SetNextLostObjectId(size_t id) noexcept {
        next_lost_object_id_ = id;
    }

    void UpdateLostObjects(std::chrono::milliseconds time_delta, const ExtraData& extra_data,
                           loot_gen::LootGenerator& loot_generator, unsigned looters);

    void SetDogSpeed(double speed) {
        dog_speed_ = speed;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    void SetBagCapacity(size_t capacity) noexcept {
        bag_capacity_ = capacity;
    }

    size_t GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;
    using RoadCells = std::unordered_map<CellKey, std::vector<size_t>, CellKeyHasher>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    RoadCells road_cells_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    LostObjects lost_objects_;
    size_t next_lost_object_id_ = 0;
    double dog_speed_ = 0.0;
    size_t bag_capacity_ = 3;
};

class ExtraData {
public:
    using LootTypes = std::vector<LootType>;

    void AddLootTypes(const Map::Id& id, LootTypes loot_types) {
        map_id_to_loot_types_.emplace(id, std::move(loot_types));
    }

    const LootTypes* FindLootTypes(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_loot_types_.find(id); it != map_id_to_loot_types_.end()) {
            return &it->second;
        }
        return nullptr;
    }

private:
    std::unordered_map<Map::Id, LootTypes, util::TaggedHasher<Map::Id>> map_id_to_loot_types_;
};

class Dog {
public:
    using Bag = std::vector<BagItem>;

    explicit Dog(std::string name, double default_speed = 0.0)
        : name_(std::move(name))
        , default_speed_(default_speed) {
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    double GetDefaultSpeed() const noexcept {
        return default_speed_;
    }

    Position GetPosition() const noexcept {
        return position_;
    }

    Speed GetSpeed() const noexcept {
        return speed_;
    }

    Direction GetDirection() const noexcept {
        return direction_;
    }

    void SetPosition(Position position) noexcept {
        position_ = position;
    }

    void SetSpeed(Speed speed) noexcept {
        speed_ = speed;
    }

    void SetDirection(Direction direction) noexcept {
        direction_ = direction;
    }

    void AddToBag(const BagItem& item) {
        bag_.push_back(item);
    }

    const Bag& GetBag() const noexcept {
        return bag_;
    }

    void ClearBag() noexcept {
        bag_.clear();
    }

    void AddScore(int64_t score) noexcept {
        score_ += score;
    }

    int64_t GetScore() const noexcept {
        return score_;
    }

private:
    std::string name_;
    double default_speed_;
    Position position_;
    Speed speed_;
    Direction direction_{Direction::NORTH};
    Bag bag_;
    int64_t score_ = 0;
};

class GameSession {
public:
    explicit GameSession(Map* map) : map_(map) {
    }

    void AddDog(std::unique_ptr<Dog> dog);

    Map* GetMap() noexcept {
        return map_;
    }

    const Map* GetMap() const noexcept {
        return map_;
    }

    std::vector<std::unique_ptr<Dog>>& GetDogs() noexcept {
        return dogs_;
    }

    const std::vector<std::unique_ptr<Dog>>& GetDogs() const noexcept {
        return dogs_;
    }

private:
    std::vector<std::unique_ptr<Dog>> dogs_;
    Map* map_ = nullptr;
};

class Game {
public:
    using Maps = std::vector<Map>;
    // deque: push_back does not invalidate references/pointers to existing sessions,
    // which app::Player::session_ relies on.
    using Sessions = std::deque<GameSession>;
    using TimeDelta = int64_t;  // milliseconds

    void AddMap(Map map);

    GameSession* FindSession(const Map::Id& map_id);
    GameSession& AddSession(Map* map);

    void SetTimeDelta(TimeDelta delta) noexcept {
        time_delta_ = delta;
    }

    TimeDelta GetTimeDelta() const noexcept {
        return time_delta_;
    }

    void Tick(TimeDelta delta_ms);

    void SetDefaultDogSpeed(double speed) noexcept {
        default_dog_speed_ = speed;
    }

    double GetDefaultDogSpeed() const noexcept {
        return default_dog_speed_;
    }

    void SetLootGeneratorPeriod(double period) noexcept {
        loot_generator_period_ = period;
    }

    double GetLootGeneratorPeriod() const noexcept {
        return loot_generator_period_;
    }

    void SetLootGeneratorProbability(double probability) noexcept {
        loot_generator_probability_ = probability;
    }

    double GetLootGeneratorProbability() const noexcept {
        return loot_generator_probability_;
    }

    void SetLootGenerator(TimeDelta period_ms, double probability) {
        loot_generator_ = loot_gen::LootGenerator{std::chrono::milliseconds{period_ms}, probability};
    }

    void SetDefaultBagCapacity(size_t capacity) noexcept {
        default_bag_capacity_ = capacity;
    }

    size_t GetDefaultBagCapacity() const noexcept {
        return default_bag_capacity_;
    }

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Sessions& GetSessions() const noexcept {
        return sessions_;
    }

    void SetStateSavePeriod(TimeDelta period_ms) noexcept {
        state_save_period_ms_ = period_ms;
    }

    void SetStateSaveCallback(std::function<void()> callback) {
        state_save_callback_ = std::move(callback);
    }

    Map* FindMap(const Map::Id& id) noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    ExtraData& GetExtraData() noexcept {
        return extra_data_;
    }

    const ExtraData& GetExtraData() const noexcept {
        return extra_data_;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    Sessions sessions_;
    MapIdToIndex map_id_to_index_;
    ExtraData extra_data_;
    double default_dog_speed_ = 1.0;
    size_t default_bag_capacity_ = 3;
    double loot_generator_period_ = 0.0;
    double loot_generator_probability_ = 0.0;
    loot_gen::LootGenerator loot_generator_{std::chrono::milliseconds{0}, 0.0};
    TimeDelta time_delta_ = 0;

    TimeDelta state_save_period_ms_ = 0;
    TimeDelta time_since_save_ms_ = 0;
    std::function<void()> state_save_callback_;
};

} 
