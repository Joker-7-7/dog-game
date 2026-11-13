#include "utils.h"

#include <algorithm>
#include <random>

namespace fs = std::filesystem;

namespace utils
{

namespace {

model::Position ToPosition(const model::Point& point)
{
    return {static_cast<double>(point.x), static_cast<double>(point.y)};
}

} // namespace

model::Position GetSpawnPoint(const model::Map& map, bool randomize)
{
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {};
    }

    if (!randomize) {
        return ToPosition(roads.front().GetStart());
    }

    static thread_local std::mt19937 gen{std::random_device{}()};

    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const model::Road& road = roads.at(road_dist(gen));

    if (road.IsHorizontal()) {
        const double lo = static_cast<double>(std::min(road.GetStart().x, road.GetEnd().x));
        const double hi = static_cast<double>(std::max(road.GetStart().x, road.GetEnd().x));
        std::uniform_real_distribution<double> point_dist(lo, hi);
        return {point_dist(gen), static_cast<double>(road.GetStart().y)};
    }

    const double lo = static_cast<double>(std::min(road.GetStart().y, road.GetEnd().y));
    const double hi = static_cast<double>(std::max(road.GetStart().y, road.GetEnd().y));
    std::uniform_real_distribution<double> point_dist(lo, hi);
    return {static_cast<double>(road.GetStart().x), point_dist(gen)};
}

bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

StringResponse MakeResponse(
    http::status status, 
    std::string_view body, 
    unsigned http_version,
    bool keep_alive,
    std::string_view content_type)
{
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}
}
