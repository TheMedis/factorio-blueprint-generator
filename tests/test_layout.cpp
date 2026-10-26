#include "layout.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace factory;
using enum factory::EntityType;
using enum factory::Direction;
using Catch::Matchers::ContainsSubstring;

namespace {
Layout make(std::vector<Entity> es) {
    Config c;
    return Layout(c.grid_size, c.input_chest, c.output_chest, std::move(es));
}
}  // namespace

TEST_CASE("empty layout is valid", "[layout]") {
    Config c;
    Layout l = Layout::empty(c);
    CHECK_FALSE(l.validate(c.max_entities).has_value());
    CHECK(l.size() == 10);
    CHECK(l.input_chest() == Vec{0, 4});
    CHECK(l.output_chest() == Vec{9, 4});
}

TEST_CASE("assembler occupies its 3x3 footprint", "[layout]") {
    Layout l = make({{Assembler, 2, 3, North}});
    std::vector<int> occ = l.occupancy();
    for (int dy = 0; dy < 3; ++dy)
        for (int dx = 0; dx < 3; ++dx) CHECK(occ[(3 + dy) * 10 + 2 + dx] == 0);
    CHECK(occ[4 * 10 + 0] == kInputChest);
    CHECK(occ[4 * 10 + 9] == kOutputChest);
    CHECK(occ[1 * 10 + 1] == kEmpty);
}

TEST_CASE("validate rejects entities outside the grid", "[layout]") {
    auto err = make({{Assembler, 8, 0, North}}).validate(60);
    REQUIRE(err.has_value());
    CHECK_THAT(*err, ContainsSubstring("entity 0") && ContainsSubstring("outside the grid"));
}

TEST_CASE("validate names both overlapping entities", "[layout]") {
    auto err = make({{Belt, 3, 4, East}, {Assembler, 2, 3, North}}).validate(60);
    REQUIRE(err.has_value());
    CHECK_THAT(*err, ContainsSubstring("entity 1") && ContainsSubstring("overlaps entity 0"));
}

TEST_CASE("validate rejects entities on a chest", "[layout]") {
    auto err = make({{Inserter, 0, 4, East}}).validate(60);
    REQUIRE(err.has_value());
    CHECK_THAT(*err, ContainsSubstring("input chest"));
}

TEST_CASE("validate rejects too many entities", "[layout]") {
    std::vector<Entity> es;
    for (int x = 0; x < 5; ++x) es.push_back({Belt, x, 0, East});
    auto err = make(es).validate(4);
    REQUIRE(err.has_value());
    CHECK_THAT(*err, ContainsSubstring("too many entities"));
}

TEST_CASE("repair keeps earlier entities and drops conflicts", "[layout]") {
    Layout l = make({{Belt, 3, 4, East}, {Assembler, 2, 3, North}, {Inserter, 1, 4, East}, {Belt, 12, 0, East}});
    l.repair(60);
    CHECK(l.entities() == std::vector<Entity>{{Belt, 3, 4, East}, {Inserter, 1, 4, East}});
    CHECK_FALSE(l.validate(60).has_value());
}

TEST_CASE("repair truncates to the entity limit", "[layout]") {
    std::vector<Entity> es;
    for (int y = 0; y < 10; ++y)
        for (int x = 1; x < 9; ++x) es.push_back({Belt, x, y, East});  // 80 belts, none on a chest
    Layout l = make(es);
    l.repair(60);
    CHECK(l.entities().size() == 60);
    CHECK_FALSE(l.validate(60).has_value());
}

TEST_CASE("json round trip", "[layout]") {
    Layout l = make({{Inserter, 1, 4, East}, {Assembler, 2, 3, North}, {Belt, 6, 4, South}});
    nlohmann::json j = layout_to_json(l);
    CHECK(j["format"] == "factory-layout-v1");
    CHECK(j["entities"][0]["name"] == "wooden-chest");
    CHECK(j["entities"][0]["position"]["x"] == 0.5);
    CHECK(j["entities"][3]["name"] == "assembling-machine-1");
    CHECK(j["entities"][3]["position"]["x"] == 3.5);  // centre of a 3x3 at x = 2
    CHECK(j["entities"][4]["direction"] == "south");
    CHECK(layout_from_json(j, 60) == l);
}

TEST_CASE("json round trip keeps non-default chests", "[layout]") {
    Layout l(10, {3, 3}, {7, 7}, {{Belt, 0, 0, West}});
    Layout back = layout_from_json(layout_to_json(l), 60);
    CHECK(back.input_chest() == Vec{3, 3});
    CHECK(back.output_chest() == Vec{7, 7});
    CHECK(back == l);
}

TEST_CASE("json errors are readable", "[layout]") {
    nlohmann::json good = layout_to_json(make({{Belt, 5, 5, East}}));

    nlohmann::json unknown = good;
    unknown["entities"][2]["name"] = "splitter";
    REQUIRE_THROWS_WITH(layout_from_json(unknown, 60), ContainsSubstring("entity #3") && ContainsSubstring("unknown entity name"));

    nlohmann::json misaligned = good;
    misaligned["entities"][2]["position"]["x"] = 5.25;
    REQUIRE_THROWS_WITH(layout_from_json(misaligned, 60), ContainsSubstring("not aligned"));

    nlohmann::json no_output = good;
    no_output["entities"].erase(1);
    REQUIRE_THROWS_WITH(layout_from_json(no_output, 60), ContainsSubstring("output chest"));

    nlohmann::json overlap = good;
    overlap["entities"][2]["position"] = {{"x", 0.5}, {"y", 4.5}};
    REQUIRE_THROWS_WITH(layout_from_json(overlap, 60), ContainsSubstring("invalid layout") && ContainsSubstring("input chest"));

    nlohmann::json bad_dir = good;
    bad_dir["entities"][2]["direction"] = "up";
    REQUIRE_THROWS_WITH(layout_from_json(bad_dir, 60), ContainsSubstring("direction"));

    REQUIRE_THROWS_WITH(layout_from_json(nlohmann::json::object(), 60), ContainsSubstring("malformed layout JSON"));
}

TEST_CASE("json errors for missing or mistyped keys name the entity", "[layout]") {
    nlohmann::json good = layout_to_json(make({{Belt, 5, 5, East}}));

    nlohmann::json no_dir = good;
    no_dir["entities"][2].erase("direction");
    REQUIRE_THROWS_WITH(layout_from_json(no_dir, 60), ContainsSubstring("entity #3") && ContainsSubstring("direction"));

    nlohmann::json no_role = good;
    no_role["entities"][0].erase("role");
    REQUIRE_THROWS_WITH(layout_from_json(no_role, 60), ContainsSubstring("entity #1") && ContainsSubstring("role"));

    nlohmann::json string_x = good;
    string_x["entities"][2]["position"]["x"] = "5.5";
    REQUIRE_THROWS_WITH(layout_from_json(string_x, 60), ContainsSubstring("entity #3"));
}

TEST_CASE("json rejects out-of-range coordinates and grid sizes", "[layout]") {
    nlohmann::json good = layout_to_json(make({{Belt, 5, 5, East}}));

    nlohmann::json overflow = good;
    overflow["entities"][2]["position"]["x"] = 2147483647.5;
    REQUIRE_THROWS_WITH(layout_from_json(overflow, 60), ContainsSubstring("entity #3") && ContainsSubstring("outside the grid"));

    nlohmann::json wraps = good;
    wraps["entities"][2]["position"]["x"] = 4294967297.5;  // would wrap to x = 1 if narrowed blindly
    REQUIRE_THROWS_WITH(layout_from_json(wraps, 60), ContainsSubstring("outside the grid"));

    nlohmann::json negative = good;
    negative["entities"][2]["position"]["x"] = -0.5;
    REQUIRE_THROWS_WITH(layout_from_json(negative, 60), ContainsSubstring("outside the grid"));

    nlohmann::json huge_grid = good;
    huge_grid["grid_size"] = 60000;
    REQUIRE_THROWS_WITH(layout_from_json(huge_grid, 60), ContainsSubstring("grid_size"));

    nlohmann::json float_grid = good;
    float_grid["grid_size"] = 1e10;
    REQUIRE_THROWS_WITH(layout_from_json(float_grid, 60), ContainsSubstring("grid_size"));
}
