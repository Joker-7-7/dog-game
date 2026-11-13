#include "api_request_handler.h"
#include "utils.h"

#include <boost/json.hpp>

#include <algorithm>
#include <optional>
#include <random>
#include <string_view>

using namespace http_handler;

namespace json = boost::json;
namespace fs = std::filesystem;

namespace
{
    json::value SerializeMap(const model::Map& map, const model::ExtraData& extra_data)
    {
        json::array roads;
        for (const auto& road : map.GetRoads()) {
            if (road.IsHorizontal()) {
                roads.push_back({
                    {"x0", road.GetStart().x},
                    {"y0", road.GetStart().y},
                    {"x1", road.GetEnd().x}
                });
            } else {
                roads.push_back({
                    {"x0", road.GetStart().x},
                    {"y0", road.GetStart().y},
                    {"y1", road.GetEnd().y}
                });
            }
        }

        json::array buildings;
        for (const auto& building : map.GetBuildings()) {
            const auto& bounds = building.GetBounds();
            buildings.push_back({
                {"x", bounds.position.x},
                {"y", bounds.position.y},
                {"w", bounds.size.width},
                {"h", bounds.size.height}
            });
        }

        json::array offices;
        for (const auto& office : map.GetOffices()) {
            offices.push_back({
                {"id", *office.GetId()},
                {"x", office.GetPosition().x},
                {"y", office.GetPosition().y},
                {"offsetX", office.GetOffset().dx},
                {"offsetY", office.GetOffset().dy}
            });
        }

        json::array loot_types;
        if (const auto* config_loot_types = extra_data.FindLootTypes(map.GetId())) {
            for (const auto& loot_type : *config_loot_types) {
                json::object loot_obj{
                    {"name", loot_type.name},
                    {"file", loot_type.file},
                    {"type", loot_type.type}
                };
                if (loot_type.rotation.has_value()) {
                    loot_obj["rotation"] = *loot_type.rotation;
                }
                if (loot_type.color.has_value()) {
                    loot_obj["color"] = *loot_type.color;
                }
                if (loot_type.value.has_value()) {
                    loot_obj["value"] = *loot_type.value;
                }
                loot_obj["scale"] = loot_type.scale;
                loot_types.push_back(std::move(loot_obj));
            }
        }

        return json::object{
            {"id", *map.GetId()},
            {"name", map.GetName()},
            {"bagCapacity", map.GetBagCapacity()},
            {"roads", std::move(roads)},
            {"buildings", std::move(buildings)},
            {"offices", std::move(offices)},
            {"lootTypes", std::move(loot_types)}
        };
    }

    std::string_view SerializeDirection(model::Direction direction)
    {
        switch (direction) {
            case model::Direction::NORTH: return "U";
            case model::Direction::SOUTH: return "D";
            case model::Direction::WEST: return "L";
            case model::Direction::EAST: return "R";
        }
        return "U";
    }

    StringResponse MakeApiResponse(
        const StringRequest& req,
        http::status status,
        std::string_view body)
    {
        auto response = utils::MakeResponse(
            status, body, req.version(), req.keep_alive(),
            utils::ContentType::APP_JSON);
        response.set(http::field::cache_control, "no-cache");
        return response;
    }

    std::optional<app::Token> ExtractToken(const StringRequest& req)
    {
        const auto it = req.find(http::field::authorization);
        if (it == req.end()) {
            return std::nullopt;
        }

        const std::string_view auth_value = it->value();
        constexpr std::string_view prefix = "Bearer ";
        if (auth_value.size() <= prefix.size()
            || auth_value.substr(0, prefix.size()) != prefix) {
            return std::nullopt;
        }

        constexpr size_t kTokenLength = 32;
        const auto token = auth_value.substr(prefix.size());
        if (token.size() != kTokenLength) {
            return std::nullopt;
        }

        return app::Token{std::string(token)};
    }
}

 StringResponse ApiRequestHandler::operator()(const StringRequest& req)
 {
    fs::path path{std::string(req.target())};
    const fs::path maps_path = "/api/v1/maps";
    const fs::path join_path = "/api/v1/game/join";
    const fs::path players_path = "/api/v1/game/players";
    const fs::path state_path = "/api/v1/game/state";
    const fs::path action_path = "/api/v1/game/player/action";
    const fs::path tick_path = "/api/v1/game/tick";

    if (path == join_path) {
        return HandleJoin(req);
    }

    if (path == maps_path) {
        return HandleMaps(req);
    }

    if (path == players_path) {
        return HandlePlayers(req);
    }

    if (path == state_path) {
        return HandleState(req);
    }

    if (path == action_path) {
        return HandleAction(req);
    }

    if (path == tick_path) {
        return HandleTick(req);
    }

    if (utils::IsSubPath(path, maps_path)) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto response = MakeApiResponse(
                req, http::status::method_not_allowed, utils::InvalidMethod);
            response.set(http::field::allow, "GET, HEAD");
            return response;
        }

        const auto map_id =
            path.string().substr(maps_path.string().size() + 1);

        const auto* map = game_.FindMap(model::Map::Id(map_id));
        if (!map) {
            return MakeApiResponse(req, http::status::not_found, utils::MapNotFound);
        }

        return MakeApiResponse(
            req,
            http::status::ok,
            json::serialize(SerializeMap(*map, game_.GetExtraData()))
        );
    }

    return MakeApiResponse(req, http::status::bad_request, utils::BadRequest);
}

StringResponse ApiRequestHandler::HandleMaps(const StringRequest& req)
{
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto response = MakeApiResponse(
            req, http::status::method_not_allowed, utils::InvalidMethod);
        response.set(http::field::allow, "GET, HEAD");
        return response;
    }

    json::array result;

    for (const auto& map : game_.GetMaps()) {
        result.push_back({
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }

    return MakeApiResponse(req, http::status::ok, json::serialize(result));
}

StringResponse ApiRequestHandler::HandleJoin(const StringRequest& req)
{
    if (req.method() != http::verb::post) {
        auto response = MakeApiResponse(
            req, http::status::method_not_allowed, utils::InvalidMethod);
        response.set(http::field::allow, "POST");
        return response;
    }

    const auto body = json::parse(req.body());
    if (body.is_object() == false) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    const auto& obj = body.as_object();
    const auto* name = obj.if_contains("userName");
    const auto* map_id = obj.if_contains("mapId");
    if (name == nullptr || name->is_string() == false || name->as_string().empty()
        || map_id == nullptr || map_id->is_string() == false ) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    auto* map = game_.FindMap(model::Map::Id{std::string(map_id->as_string())});
    if (map == nullptr) {
        return MakeApiResponse(req, http::status::not_found, utils::MapNotFound);
    }

    model::GameSession* session = game_.FindSession(map->GetId());
    if (session == nullptr) {
        session = &game_.AddSession(map);
    }

    auto dog = std::make_unique<model::Dog>(
        std::string(name->as_string()),
        map->GetDogSpeed());
    dog->SetPosition(utils::GetSpawnPoint(*map, randomizeSpawnPoints));

    app::Player& player = players_.AddPlayer(dog.get(), session);
    const app::Token token = players_.FindToken(&player);
    session->AddDog(std::move(dog));
    
    return MakeApiResponse(req, http::status::ok, json::serialize(json::object{
        {"authToken", *token},
        {"playerId", *player.id_}
    }));
}

StringResponse ApiRequestHandler::HandlePlayers(const StringRequest& req)
{
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto response = MakeApiResponse(
            req, http::status::method_not_allowed, utils::InvalidMethod);
        response.set(http::field::allow, "GET, HEAD");
        return response;
    }

    const auto token = ExtractToken(req);
    if (!token) {
        return MakeApiResponse(
            req, http::status::unauthorized, utils::InvalidToken);
    }

    const auto player = players_.FindByToken(*token);
    if (player == nullptr) {
        return MakeApiResponse(
            req, http::status::unauthorized, utils::UnknownToken);
    }

    json::object result;

    for (const auto& [_, player] : players_.GetPlayers()) {
        result[std::to_string(*player->id_)] = json::object{
            {"name", player->dog_->GetName()}
        };
    }
    return MakeApiResponse(req, http::status::ok, json::serialize(result));
}

StringResponse ApiRequestHandler::HandleState(const StringRequest& req)
{
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto response = MakeApiResponse(
            req, http::status::method_not_allowed, utils::InvalidMethod);
        response.set(http::field::allow, "GET, HEAD");
        return response;
    }

    const auto token = ExtractToken(req);
    if (!token) {
        return MakeApiResponse(
            req, http::status::unauthorized, utils::InvalidToken);
    }

    const auto player = players_.FindByToken(*token);
    if (player == nullptr) {
        return MakeApiResponse(
            req, http::status::unauthorized, utils::UnknownToken);
    }

    json::object result;

    for (const auto& [_, player] : players_.GetPlayers()) {
        const auto position = player->dog_->GetPosition();
        const auto speed = player->dog_->GetSpeed();

        json::array bag;
        for (const auto& item : player->dog_->GetBag()) {
            bag.push_back(json::object{
                {"id", item.id},
                {"type", item.type}
            });
        }

        result[std::to_string(*player->id_)] = json::object{
            {"pos", json::array{position.x, position.y}},
            {"speed", json::array{speed.vx, speed.vy}},
            {"dir", SerializeDirection(player->dog_->GetDirection())},
            {"score", player->dog_->GetScore()},
            {"bag", std::move(bag)}
        };
    }

    json::object lost_objects;
    const auto* map = player->session_->GetMap();
    if (map != nullptr) {
        for (const auto& lost_object : map->GetLostObjects()) {
            lost_objects[std::to_string(lost_object.id)] = json::object{
                {"type", lost_object.type_index},
                {"pos", json::array{lost_object.position.x, lost_object.position.y}}
            };
        }
    }

    return MakeApiResponse(
        req, http::status::ok,
        json::serialize(json::object{
            {"players", result},
            {"lostObjects", lost_objects}
        }));
}

StringResponse ApiRequestHandler::HandleAction(const StringRequest& req)
{
    if (req.method() != http::verb::post) {
        auto response = MakeApiResponse(
            req, http::status::method_not_allowed, utils::InvalidMethod);
        response.set(http::field::allow, "POST");
        return response;
    }

    const auto content_type = req.find(http::field::content_type);
    if (content_type == req.end()
        || content_type->value() != utils::ContentType::APP_JSON) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    const auto token = ExtractToken(req);
    if (!token) {
        return MakeApiResponse(
            req, http::status::unauthorized, utils::InvalidToken);
    }

    const auto player = players_.FindByToken(*token);
    if (player == nullptr) {
        return MakeApiResponse(
            req, http::status::unauthorized, utils::UnknownToken);
    }

    json::value body;
    try {
        body = json::parse(req.body());
    } catch (const std::exception&) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    if (!body.is_object()) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    const auto* move = body.as_object().if_contains("move");
    if (move == nullptr || !move->is_string()) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    const double base_speed = player->dog_->GetDefaultSpeed();
    const auto move_value = move->as_string();

    model::Speed speed{0.0, 0.0};
    if (move_value == "L") {
        speed = {-base_speed, 0.0};
        player->dog_->SetDirection(model::Direction::WEST);
    } else if (move_value == "R") {
        speed = {base_speed, 0.0};
        player->dog_->SetDirection(model::Direction::EAST);
    } else if (move_value == "U") {
        speed = {0.0, -base_speed};
        player->dog_->SetDirection(model::Direction::NORTH);
    } else if (move_value == "D") {
        speed = {0.0, base_speed};
        player->dog_->SetDirection(model::Direction::SOUTH);
    } else if (!move_value.empty()) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    player->dog_->SetSpeed(speed);

    return MakeApiResponse(
        req, http::status::ok, json::serialize(json::object{}));
}

StringResponse ApiRequestHandler::HandleTick(const StringRequest& req)
{
    if(isTickMode) {
        return MakeApiResponse(req, http::status::bad_request, utils::BadRequest);
    }

    if (req.method() != http::verb::post) {
        auto response = MakeApiResponse(
            req, http::status::method_not_allowed, utils::InvalidMethod);
        response.set(http::field::allow, "POST");
        return response;
    }

    const auto content_type = req.find(http::field::content_type);
    if (content_type == req.end()
        || content_type->value() != utils::ContentType::APP_JSON) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    json::value body;
    try {
        body = json::parse(req.body());
    } catch (const std::exception&) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    if (!body.is_object()) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    const auto* time_delta = body.as_object().if_contains("timeDelta");
    if (time_delta == nullptr
        || !time_delta->is_int64()
        || time_delta->as_int64() < 0) {
        return MakeApiResponse(
            req, http::status::bad_request, utils::InvalidArgument);
    }

    const auto delta = time_delta->as_int64();
    game_.SetTimeDelta(delta);
    game_.Tick(delta);

    return MakeApiResponse(
        req, http::status::ok, json::serialize(json::object{}));
}
