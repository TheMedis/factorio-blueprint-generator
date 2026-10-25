#include "entities.hpp"

namespace factory {

Vec dir_vec(Direction d) {
    switch (d) {
    case Direction::North: return {0, -1};
    case Direction::East: return {1, 0};
    case Direction::South: return {0, 1};
    case Direction::West: return {-1, 0};
    }
    return {0, 0};
}

Direction opposite(Direction d) { return Direction((int(d) + 2) % 4); }
Direction rotate_cw(Direction d) { return Direction((int(d) + 1) % 4); }
Direction rotate_ccw(Direction d) { return Direction((int(d) + 3) % 4); }

int entity_size(EntityType t) { return t == EntityType::Assembler ? 3 : 1; }

const char* factorio_name(EntityType t) {
    switch (t) {
    case EntityType::Assembler: return "assembling-machine-1";
    case EntityType::Inserter: return "inserter";
    case EntityType::Belt: return "transport-belt";
    }
    return "";
}

std::optional<EntityType> type_from_factorio_name(const std::string& name) {
    for (EntityType t : {EntityType::Assembler, EntityType::Inserter, EntityType::Belt})
        if (name == factorio_name(t)) return t;
    return std::nullopt;
}

const char* direction_name(Direction d) {
    static constexpr const char* names[] = {"north", "east", "south", "west"};
    return names[int(d)];
}

std::optional<Direction> direction_from_name(const std::string& name) {
    for (Direction d : {Direction::North, Direction::East, Direction::South, Direction::West})
        if (name == direction_name(d)) return d;
    return std::nullopt;
}

}  // namespace factory
