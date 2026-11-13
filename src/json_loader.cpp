#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json = boost::json;

namespace json_loader
{

namespace {

void SetupBuildings(model::Map& map, const json::object& obj) {
    auto buildings = obj.at("buildings").as_array();
    for(auto& building : buildings)
    {
        auto objBuild = building.as_object();
        model::Rectangle rect(model::Point(
            static_cast<model::Coord>(objBuild["x"].as_int64()),
            static_cast<model::Coord>(objBuild["y"].as_int64())),
                        model::Size(
                            static_cast<model::Dimension>(objBuild["w"].as_int64()),
                            static_cast<model::Dimension>(objBuild["h"].as_int64())));
        map.AddBuilding(model::Building(std::move(rect)));
    }
}

void SetupOffices(model::Map& map, const json::object& obj) {
    if (obj.contains("offices")) {
        auto offices = obj.at("offices").as_array();
        for(auto& office : offices)
        {
            auto objOffice = office.as_object();
            map.AddOffice(model::Office(
                model::Office::Id(std::string(objOffice.at("id").as_string())),
                model::Point{
                    static_cast<model::Coord>(objOffice.at("x").as_int64()),
                    static_cast<model::Coord>(objOffice.at("y").as_int64())
                },
                model::Offset{
                    static_cast<model::Coord>(objOffice.at("offsetX").as_int64()),
                    static_cast<model::Coord>(objOffice.at("offsetY").as_int64())
                }));
        }
    }
}

void CreateRoads(model::Map& map, const json::array& roads) {
    for(auto& road : roads)
    {
        auto roadObj = road.as_object();
        if(roadObj.contains("x1"))
        {
            map.AddRoad(model::Road{
                model::Road::HORIZONTAL,
                model::Point{
                    static_cast<model::Coord>(roadObj["x0"].as_int64()),
                    static_cast<model::Coord>(roadObj["y0"].as_int64())
                },
                static_cast<model::Coord>(roadObj["x1"].as_int64())
            });
        }
        else if (roadObj.contains("y1"))
        {
            map.AddRoad(model::Road{
                model::Road::VERTICAL,
                model::Point{
                    static_cast<model::Coord>(roadObj["x0"].as_int64()),
                    static_cast<model::Coord>(roadObj["y0"].as_int64())
                },
                static_cast<model::Coord>(roadObj["y1"].as_int64())
            });
        }
    }
}

void SetupLootTypes(model::ExtraData& extra_data, const model::Map::Id& id, const json::object& obj) {
    if (!obj.contains("lootTypes")) {
        return;
    }
    model::ExtraData::LootTypes loot_types;
    for(auto& loot_type : obj.at("lootTypes").as_array())
    {
        auto objLootType = loot_type.as_object();
        model::LootType loot{
            std::string(objLootType.at("name").as_string()),
            std::string(objLootType.at("file").as_string()),
            std::string(objLootType.at("type").as_string())
        };
        if (objLootType.contains("rotation")) {
            loot.rotation = static_cast<int>(objLootType.at("rotation").as_int64());
        }
        if (objLootType.contains("color")) {
            loot.color = std::string(objLootType.at("color").as_string());
        }
        if (objLootType.contains("value")) {
            loot.value = objLootType.at("value").as_int64();
        }
        loot.scale = objLootType.at("scale").as_double();
        loot_types.push_back(std::move(loot));
    }
    extra_data.AddLootTypes(id, std::move(loot_types));
}

} // namespace

void SetupMaps(model::Game& game, const json::array& maps) {
    for(const auto& m : maps)
    {
        const auto& obj = m.as_object();

        auto id = std::string(obj.at("id").as_string());
        auto name = std::string(obj.at("name").as_string());
        model::Map map(model::Map::Id(id), name);

        map.SetDogSpeed(obj.contains("dogSpeed")
            ? obj.at("dogSpeed").as_double()
            : game.GetDefaultDogSpeed());

        map.SetBagCapacity(obj.contains("bagCapacity")
            ? static_cast<size_t>(obj.at("bagCapacity").as_int64())
            : game.GetDefaultBagCapacity());

        CreateRoads(map, obj.at("roads").as_array());

        SetupBuildings(map, obj);
        SetupOffices(map, obj);
        SetupLootTypes(game.GetExtraData(), map.GetId(), obj);

        game.AddMap(std::move(map));
    }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    auto fileObj = json::parse(ss.str());

    auto maps = fileObj.at("maps").as_array();
    model::Game game;

    if (fileObj.as_object().contains("defaultDogSpeed")) {
        game.SetDefaultDogSpeed(fileObj.at("defaultDogSpeed").as_double());
    }

    if (fileObj.as_object().contains("defaultBagCapacity")) {
        game.SetDefaultBagCapacity(
            static_cast<size_t>(fileObj.at("defaultBagCapacity").as_int64()));
    }

    if (fileObj.as_object().contains("lootGeneratorConfig")) {
        const auto& loot_cfg = fileObj.at("lootGeneratorConfig").as_object();
        const auto period = loot_cfg.at("period").as_double();
        const auto probability = loot_cfg.at("probability").as_double();
        game.SetLootGeneratorPeriod(period);
        game.SetLootGeneratorProbability(probability);
        game.SetLootGenerator(static_cast<model::Game::TimeDelta>(period * 1000), probability);
    }

    SetupMaps(game, maps);

    return game;
}

}
