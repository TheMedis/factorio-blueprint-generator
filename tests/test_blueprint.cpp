#include "blueprint.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace factory;
using enum factory::EntityType;
using enum factory::Direction;
using Catch::Matchers::ContainsSubstring;
using nlohmann::json;

namespace {
Layout sample() {
    Config c;
    return Layout(c.grid_size, c.input_chest, c.output_chest,
                  {{Inserter, 1, 4, East}, {Assembler, 2, 3, North}, {Inserter, 5, 4, North}, {Belt, 6, 4, South},
                   {Belt, 7, 4, North}});
}

// The blueprint entity with this name centred at (x, y); fails the test if absent.
json find(const json& bp, const std::string& name, double x, double y) {
    for (const json& e : bp["blueprint"]["entities"])
        if (e["name"] == name && e["position"]["x"] == x && e["position"]["y"] == y) return e;
    FAIL("no " << name << " at " << x << "," << y);
    return {};
}

int count_named(const json& bp, const std::string& name) {
    int n = 0;
    for (const json& e : bp["blueprint"]["entities"]) n += e["name"] == name;
    return n;
}
}  // namespace

TEST_CASE("blueprint string round-trips through zlib and base64", "[blueprint]") {
    const std::string s = blueprint_string(sample());
    REQUIRE(s.size() > 1);
    CHECK(s[0] == '0');
    CHECK(s.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/=") == std::string::npos);
    CHECK(decode_blueprint_string(s) == blueprint_json(sample()));
}

TEST_CASE("blueprint header targets Factorio 2.0", "[blueprint]") {
    const json bp = blueprint_json(sample());
    CHECK(bp["blueprint"]["item"] == "blueprint");
    CHECK(bp["blueprint"]["version"] == 562949954076672ULL);  // 2.0.10.0
    CHECK(bp["blueprint"]["icons"][0]["signal"]["name"] == "iron-gear-wheel");
}

TEST_CASE("blueprint maps entities, directions and recipes", "[blueprint]") {
    const json bp = blueprint_json(sample());
    // Inserters: Factorio stores the pickup side, so directions are reversed (16-way numbering).
    CHECK(find(bp, "inserter", 1.5, 4.5)["direction"] == 12);  // drops east -> faces west
    CHECK(find(bp, "inserter", 5.5, 4.5)["direction"] == 8);   // drops north -> faces south
    // Belts keep their travel direction; north (0) is omitted like in game exports.
    CHECK(find(bp, "transport-belt", 6.5, 4.5)["direction"] == 8);
    CHECK_FALSE(find(bp, "transport-belt", 7.5, 4.5).contains("direction"));
    // Assemblers are centred on their 3x3 footprint and come with the gear recipe.
    const json am = find(bp, "assembling-machine-1", 3.5, 4.5);
    CHECK(am["recipe"] == "iron-gear-wheel");
    CHECK_FALSE(am.contains("direction"));
}

TEST_CASE("blueprint chests: infinity plates in, wooden chest out", "[blueprint]") {
    const json bp = blueprint_json(sample());
    const json in = find(bp, "infinity-chest", 0.5, 4.5);
    const json& filter = in["infinity_settings"]["filters"][0];
    CHECK(filter["name"] == "iron-plate");
    CHECK(filter["mode"] == "at-least");
    CHECK(filter["count"] == 100);
    CHECK(filter["index"] == 1);
    find(bp, "wooden-chest", 9.5, 4.5);
    CHECK(count_named(bp, "infinity-chest") == 1);
    CHECK(count_named(bp, "wooden-chest") == 1);
}

TEST_CASE("blueprint power covers the grid from outside it", "[blueprint]") {
    const Layout l = sample();
    const json bp = blueprint_json(l);
    CHECK(count_named(bp, "substation") == 2);
    CHECK(count_named(bp, "electric-energy-interface") == 2);
    std::vector<std::pair<double, double>> substations;
    for (const json& e : bp["blueprint"]["entities"]) {
        if (e["name"] != "substation" && e["name"] != "electric-energy-interface") continue;
        const double x = e["position"]["x"];
        CHECK((x < 0 || x > l.size()));  // 2x2 entities wholly outside the grid's columns
        if (e["name"] == "substation") substations.push_back({x, e["position"]["y"].get<double>()});
    }
    // Substation supply area is 18x18 around its centre: every grid tile must touch one.
    for (int ty = 0; ty < l.size(); ++ty)
        for (int tx = 0; tx < l.size(); ++tx) {
            bool covered = false;
            for (auto [sx, sy] : substations)
                covered |= tx + 1 > sx - 9 && tx < sx + 9 && ty + 1 > sy - 9 && ty < sy + 9;
            CHECK(covered);
        }
}

TEST_CASE("blueprint entity numbers are 1..n", "[blueprint]") {
    const json bp = blueprint_json(sample());
    const json& es = bp["blueprint"]["entities"];
    for (std::size_t i = 0; i < es.size(); ++i) CHECK(es[i]["entity_number"] == i + 1);
    CHECK(es.size() == 5 + 2 + 4);  // layout + chests + power
}

TEST_CASE("decoding rejects bad strings", "[blueprint]") {
    REQUIRE_THROWS_WITH(decode_blueprint_string(""), ContainsSubstring("empty"));
    REQUIRE_THROWS_WITH(decode_blueprint_string("1abcd"), ContainsSubstring("version"));
    REQUIRE_THROWS_WITH(decode_blueprint_string("0!!!!"), ContainsSubstring("base64"));
    REQUIRE_THROWS_WITH(decode_blueprint_string("0AAAA"), ContainsSubstring("zlib"));
}
