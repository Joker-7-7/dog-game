#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "model.h"
#include "players.h"

namespace serialization {

// A snapshot of the server state as plain data suitable for serialization.
struct GameState {
    struct BagItemRepr {
        size_t id = 0;
        size_t type = 0;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& id;
            ar& type;
        }
    };

    struct DogRepr {
        std::string name;
        double default_speed = 0.0;
        double pos_x = 0.0;
        double pos_y = 0.0;
        double speed_x = 0.0;
        double speed_y = 0.0;
        std::string direction;  // "U", "D", "L", "R"
        std::vector<BagItemRepr> bag;
        int64_t score = 0;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& name;
            ar& default_speed;
            ar& pos_x;
            ar& pos_y;
            ar& speed_x;
            ar& speed_y;
            ar& direction;
            ar& bag;
            ar& score;
        }
    };

    struct SessionRepr {
        std::string map_id;
        std::vector<DogRepr> dogs;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& map_id;
            ar& dogs;
        }
    };

    struct LostObjectRepr {
        size_t id = 0;
        size_t type = 0;
        double pos_x = 0.0;
        double pos_y = 0.0;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& id;
            ar& type;
            ar& pos_x;
            ar& pos_y;
        }
    };

    struct MapStateRepr {
        std::string map_id;
        std::vector<LostObjectRepr> lost_objects;
        size_t next_lost_object_id = 0;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& map_id;
            ar& lost_objects;
            ar& next_lost_object_id;
        }
    };

    struct PlayerRepr {
        std::string token;
        uint64_t player_id = 0;
        std::string map_id;
        size_t dog_index = 0;

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& token;
            ar& player_id;
            ar& map_id;
            ar& dog_index;
        }
    };

    std::vector<SessionRepr> sessions;
    std::vector<MapStateRepr> maps;
    std::vector<PlayerRepr> players;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& sessions;
        ar& maps;
        ar& players;
    }
};

void SaveGameState(const model::Game& game, const app::Players& players,
                   const std::filesystem::path& path);

void LoadGameState(model::Game& game, app::Players& players,
                   const std::filesystem::path& path);

}  // namespace serialization
