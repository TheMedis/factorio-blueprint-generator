#include "render.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace factory;
using enum factory::EntityType;
using enum factory::Direction;

namespace {
Layout seed_like() {
    Config c;
    return Layout(c.grid_size, c.input_chest, c.output_chest,
                  {{Inserter, 1, 4, East}, {Assembler, 2, 3, North}, {Inserter, 5, 4, East},
                   {Belt, 6, 4, East}, {Belt, 7, 4, East}, {Inserter, 8, 4, East}});
}

int count(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (std::size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
    return n;
}
}  // namespace

TEST_CASE("ascii render", "[render]") {
    const std::string expected =
        "..........\n"
        "..........\n"
        "..........\n"
        "..AAA.....\n"
        "IeAAAe>>eO\n"
        "..AAA.....\n"
        "..........\n"
        "..........\n"
        "..........\n"
        "..........\n";
    CHECK(render_ascii(seed_like()) == expected);
    CHECK(ascii_legend().find("inserters") != std::string::npos);
}

TEST_CASE("svg render has one group per entity", "[render]") {
    const std::string svg = render_svg(seed_like());
    CHECK(svg.rfind("<svg", 0) == 0);
    CHECK(svg.find("</svg>") != std::string::npos);
    CHECK(count(svg, "class=\"chest\"") == 2);
    CHECK(count(svg, "class=\"assembler\"") == 1);
    CHECK(count(svg, "class=\"inserter\"") == 3);
    CHECK(count(svg, "class=\"belt\"") == 2);
}
