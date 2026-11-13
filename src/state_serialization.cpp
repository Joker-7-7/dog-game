#include "state_serialization.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace {

constexpr std::string_view kDirNorth = "U";
constexpr std::string_view kDirSouth = "D";
constexpr std::string_view kDirWest = "L";
constexpr std::string_view kDirEast = "R";

std::string SerializeDirection(model::Direction direction) {
    switch (direction) {
        case model::Direction::NORTH: return std::string(kDirNorth);
        case model::Direction::SOUTH: return std::string(kDirSouth);
        case model::Direction::WEST: return std::string(kDirWest);
        case model::Direction::EAST: return std::string(kDirEast);
    }
    return std::string(kDirNorth);
}

model::Direction DeserializeDirection(std::string_view direction) {
    if (direction == kDirNorth) return model::Direction::NORTH;
    if (direction == kDirSouth) return model::Direction::SOUTH;
    if (direction == kDirWest) return model::Direction::WEST;
    if (direction == kDirEast) return model::Direction::EAST;
    throw std::runtime_error("Invalid direction in state file");
}

serialization::GameState MakeSnapshot(const model::Game& game, const app::Players& players) {
    serialization::GameState state;

    for (const auto& session : game.GetSessions()) {
        const auto* map = session.GetMap();
        if (map == nullptr) {
            continue;
        }
        serialization::GameState::SessionRepr session_repr;
        session_repr.map_id = *map->GetId();
        for (const auto& dog : session.GetDogs()) {
            const auto pos = dog->GetPosition();
            const auto speed = dog->GetSpeed();
            serialization::GameState::DogRepr dog_repr;
            dog_repr.name = dog->GetName();
            dog_repr.default_speed = dog->GetDefaultSpeed();
            dog_repr.pos_x = pos.x;
            dog_repr.pos_y = pos.y;
            dog_repr.speed_x = speed.vx;
            dog_repr.speed_y = speed.vy;
            dog_repr.direction = SerializeDirection(dog->GetDirection());
            dog_repr.score = dog->GetScore();
            for (const auto& item : dog->GetBag()) {
                dog_repr.bag.push_back({item.id, item.type});
            }
            session_repr.dogs.push_back(std::move(dog_repr));
        }
        state.sessions.push_back(std::move(session_repr));
    }

    for (const auto& map : game.GetMaps()) {
        serialization::GameState::MapStateRepr map_repr;
        map_repr.map_id = *map.GetId();
        map_repr.next_lost_object_id = map.GetNextLostObjectId();
        for (const auto& obj : map.GetLostObjects()) {
            map_repr.lost_objects.push_back({
                obj.id,
                obj.type_index,
                obj.position.x,
                obj.position.y
            });
        }
        state.maps.push_back(std::move(map_repr));
    }

    for (const auto& [token, player] : players.GetPlayers()) {
        const auto* session = player->session_;
        if (session == nullptr || session->GetMap() == nullptr || player->dog_ == nullptr) {
            continue;
        }
        const auto& dogs = session->GetDogs();
        const auto dog_it = std::find_if(dogs.begin(), dogs.end(),
            [&player](const auto& dog) { return dog.get() == player->dog_; });
        if (dog_it == dogs.end()) {
            continue;
        }
        state.players.push_back({
            *token,
            *player->id_,
            *session->GetMap()->GetId(),
            static_cast<size_t>(std::distance(dogs.begin(), dog_it))
        });
    }

    return state;
}

void RestoreState(model::Game& game, app::Players& players,
                  const serialization::GameState& state) {
    for (const auto& session_repr : state.sessions) {
        auto* map = game.FindMap(model::Map::Id{session_repr.map_id});
        if (map == nullptr) {
            throw std::runtime_error("Unknown map in state file: " + session_repr.map_id);
        }
        model::GameSession* session = game.FindSession(map->GetId());
        if (session == nullptr) {
            session = &game.AddSession(map);
        }
        for (const auto& dog_repr : session_repr.dogs) {
            auto dog = std::make_unique<model::Dog>(dog_repr.name, dog_repr.default_speed);
            dog->SetPosition({dog_repr.pos_x, dog_repr.pos_y});
            dog->SetSpeed({dog_repr.speed_x, dog_repr.speed_y});
            dog->SetDirection(DeserializeDirection(dog_repr.direction));
            dog->AddScore(dog_repr.score);
            for (const auto& item : dog_repr.bag) {
                dog->AddToBag({item.id, item.type});
            }
            session->AddDog(std::move(dog));
        }
    }

    for (const auto& map_repr : state.maps) {
        auto* map = game.FindMap(model::Map::Id{map_repr.map_id});
        if (map == nullptr) {
            throw std::runtime_error("Unknown map in state file: " + map_repr.map_id);
        }
        map->SetNextLostObjectId(map_repr.next_lost_object_id);
        for (const auto& obj : map_repr.lost_objects) {
            map->AddLostObject({obj.id, obj.type, {obj.pos_x, obj.pos_y}});
        }
    }

    for (const auto& player_repr : state.players) {
        auto* session = game.FindSession(model::Map::Id{player_repr.map_id});
        if (session == nullptr || player_repr.dog_index >= session->GetDogs().size()) {
            throw std::runtime_error("Invalid player record in state file");
        }
        model::Dog* dog = session->GetDogs().at(player_repr.dog_index).get();
        players.RestorePlayer(dog, session, app::Player::Id{player_repr.player_id},
                              app::Token{player_repr.token});
    }
}

}  // namespace

namespace serialization {

void SaveGameState(const model::Game& game, const app::Players& players,
                   const std::filesystem::path& path) {
    auto tmp_path = path;
    tmp_path += ".tmp";
    {
        std::ofstream out(tmp_path);
        boost::archive::text_oarchive archive{out};
        archive << MakeSnapshot(game, players);
        out.flush();
        if (!out) {
            throw std::runtime_error("Failed to write state file: " + path.string());
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        throw std::runtime_error("Failed to write state file: " + path.string());
    }
}

void LoadGameState(model::Game& game, app::Players& players,
                   const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open state file: " + path.string());
    }

    serialization::GameState state;
    try {
        boost::archive::text_iarchive archive{in};
        archive >> state;
    } catch (const std::exception& ex) {
        throw std::runtime_error("State file is corrupted: " + path.string()
                                 + " (" + ex.what() + ")");
    }

    RestoreState(game, players, state);
}

}  // namespace serialization
