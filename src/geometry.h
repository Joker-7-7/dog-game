#pragma once
#include <cstddef>
#include <functional>

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct CellKey {
    int x = 0;
    int y = 0;

    bool operator==(const CellKey& other) const noexcept = default;
};

struct CellKeyHasher {
    size_t operator()(const CellKey& key) const noexcept {
        return std::hash<int>{}(key.x) * 37u + std::hash<int>{}(key.y);
    }
};

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Speed {
    double vx = 0.0;
    double vy = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST,
};

struct RoadBounds {
    double x_min = 0.0;
    double y_min = 0.0;
    double x_max = 0.0;
    double y_max = 0.0;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

}  // namespace model
