#include "entities.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace factory;

TEST_CASE("direction helpers", "[entities]") {
    CHECK(dir_vec(Direction::North) == Vec{0, -1});
    CHECK(dir_vec(Direction::East) == Vec{1, 0});
    CHECK(dir_vec(Direction::South) == Vec{0, 1});
    CHECK(dir_vec(Direction::West) == Vec{-1, 0});
    CHECK(opposite(Direction::North) == Direction::South);
    CHECK(opposite(Direction::West) == Direction::East);
    CHECK(rotate_cw(Direction::North) == Direction::East);
    CHECK(rotate_cw(Direction::West) == Direction::North);
    CHECK(rotate_ccw(Direction::North) == Direction::West);
    CHECK(rotate_ccw(Direction::East) == Direction::North);
}

TEST_CASE("entity sizes and Factorio names", "[entities]") {
    CHECK(entity_size(EntityType::Assembler) == 3);
    CHECK(entity_size(EntityType::Inserter) == 1);
    CHECK(entity_size(EntityType::Belt) == 1);
    CHECK(std::string(factorio_name(EntityType::Assembler)) == "assembling-machine-1");
    CHECK(std::string(factorio_name(EntityType::Inserter)) == "inserter");
    CHECK(std::string(factorio_name(EntityType::Belt)) == "transport-belt");
    CHECK(type_from_factorio_name("transport-belt") == EntityType::Belt);
    CHECK_FALSE(type_from_factorio_name("splitter").has_value());
    for (Direction d : {Direction::North, Direction::East, Direction::South, Direction::West})
        CHECK(direction_from_name(direction_name(d)) == d);
    CHECK_FALSE(direction_from_name("up").has_value());
}

TEST_CASE("tier-1 config reproduces Factorio rates", "[entities]") {
    Config c;
    CHECK(60.0 / (2 * c.inserter_swing_ticks) == Catch::Approx(0.8333).margin(1e-3));  // inserter items/s
    CHECK(60.0 / c.craft_ticks == Catch::Approx(1.0));                                // AM1 gears/s
    CHECK(60.0 / c.belt_slot_ticks == Catch::Approx(7.5));                            // items/s per lane
    CHECK(c.input_chest == Vec{0, 4});
    CHECK(c.output_chest == Vec{9, 4});
    CHECK(c.grid_size == 10);
    CHECK(c.seed_fraction == 0.0);  // GA starts from random layouts only unless --seeds
}
