#include "sim.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace factory;
using enum factory::EntityType;
using enum factory::Direction;
using Catch::Approx;

namespace {
// Chests tucked into the bottom corners so the top of the grid is free for belt tests.
Config corner_cfg() {
    Config c;
    c.input_chest = {0, 9};
    c.output_chest = {9, 9};
    return c;
}

Layout make(const Config& c, std::vector<Entity> es) {
    Layout l(c.grid_size, c.input_chest, c.output_chest, std::move(es));
    REQUIRE_FALSE(l.validate(c.max_entities).has_value());
    return l;
}

int count_on_belts(const Simulator& s, int belts) {
    int n = 0;
    for (int b = 0; b < belts; ++b)
        for (int lane = 0; lane < 2; ++lane)
            for (int slot = 0; slot < kBeltSlots; ++slot) n += s.belt_slot(b, lane, slot) != Item::None;
    return n;
}
}  // namespace

TEST_CASE("belt item advances one slot per 8 ticks and one tile per 32", "[belts]") {
    Config c = corner_cfg();
    Simulator s(make(c, {{Belt, 1, 0, East}, {Belt, 2, 0, East}, {Belt, 3, 0, East}}), c);
    s.set_belt_slot(0, 0, 0, Item::Plate);
    s.run_ticks(8);
    CHECK(s.belt_slot(0, 0, 0) == Item::None);
    CHECK(s.belt_slot(0, 0, 1) == Item::Plate);
    s.run_ticks(24);
    CHECK(s.belt_slot(1, 0, 0) == Item::Plate);
}

TEST_CASE("compressed lane moves as a unit (7.5 items/s per lane)", "[belts]") {
    Config c = corner_cfg();
    Simulator s(make(c, {{Belt, 1, 0, East}, {Belt, 2, 0, East}, {Belt, 3, 0, East}}), c);
    for (int b = 0; b < 2; ++b)
        for (int slot = 0; slot < kBeltSlots; ++slot) s.set_belt_slot(b, 0, slot, Item::Plate);
    s.run_ticks(8);
    CHECK(s.belt_slot(2, 0, 0) == Item::Plate);
    for (int slot = 0; slot < kBeltSlots; ++slot) CHECK(s.belt_slot(1, 0, slot) == Item::Plate);
    CHECK(s.belt_slot(0, 0, 0) == Item::None);
    for (int slot = 1; slot < kBeltSlots; ++slot) CHECK(s.belt_slot(0, 0, slot) == Item::Plate);
}

TEST_CASE("items stop at the end of a belt", "[belts]") {
    Config c = corner_cfg();
    Simulator s(make(c, {{Belt, 1, 0, East}}), c);
    s.set_belt_slot(0, 0, 0, Item::Plate);
    s.run_ticks(200);
    CHECK(s.belt_slot(0, 0, kBeltSlots - 1) == Item::Plate);
    CHECK(count_on_belts(s, 1) == 1);
}

TEST_CASE("head-on belts do not connect", "[belts]") {
    Config c = corner_cfg();
    Simulator s(make(c, {{Belt, 1, 0, East}, {Belt, 2, 0, West}}), c);
    s.set_belt_slot(0, 0, kBeltSlots - 1, Item::Plate);
    s.run_ticks(100);
    CHECK(s.belt_slot(0, 0, kBeltSlots - 1) == Item::Plate);
    CHECK(count_on_belts(s, 2) == 1);
}

TEST_CASE("curve preserves lanes", "[belts]") {
    Config c = corner_cfg();
    Simulator s(make(c, {{Belt, 1, 1, East}, {Belt, 2, 1, South}}), c);  // nothing behind (2,1) -> curve
    s.set_belt_slot(0, 0, kBeltSlots - 1, Item::Plate);
    s.set_belt_slot(0, 1, kBeltSlots - 1, Item::Gear);
    s.run_ticks(8);
    CHECK(s.belt_slot(1, 0, 0) == Item::Plate);
    CHECK(s.belt_slot(1, 1, 0) == Item::Gear);
}

TEST_CASE("side-load puts items on the near lane", "[belts]") {
    Config c = corner_cfg();
    // Southward line (2,0)->(2,1)->(2,2); (1,1) feeds from the west, (3,1) from the east.
    Simulator s(make(c, {{Belt, 2, 0, South}, {Belt, 2, 1, South}, {Belt, 2, 2, South},
                         {Belt, 1, 1, East}, {Belt, 3, 1, West}}), c);
    s.set_belt_slot(3, 0, kBeltSlots - 1, Item::Plate);  // from the west
    s.set_belt_slot(4, 1, kBeltSlots - 1, Item::Gear);   // from the east
    s.run_ticks(8);
    // Travelling south, west is the right lane (1) and east is the left lane (0).
    CHECK(s.belt_slot(1, 1, kBeltSlots / 2) == Item::Plate);
    CHECK(s.belt_slot(1, 0, kBeltSlots / 2) == Item::Gear);
}

TEST_CASE("inserter drops on the far lane", "[belts]") {
    SECTION("inserter north of an eastward belt -> right lane") {
        Config c = corner_cfg();
        c.input_chest = {2, 0};
        Simulator s(make(c, {{Inserter, 2, 1, South}, {Belt, 2, 2, East}}), c);
        s.run_ticks(37);
        CHECK(s.belt_slot(1, 1, kBeltSlots / 2) == Item::Plate);
        for (int slot = 0; slot < kBeltSlots; ++slot) CHECK(s.belt_slot(1, 0, slot) == Item::None);
    }
    SECTION("inserter south of an eastward belt -> left lane") {
        Config c = corner_cfg();
        c.input_chest = {2, 4};
        Simulator s(make(c, {{Inserter, 2, 3, North}, {Belt, 2, 2, East}}), c);
        s.run_ticks(37);
        CHECK(s.belt_slot(1, 0, kBeltSlots / 2) == Item::Plate);
        for (int slot = 0; slot < kBeltSlots; ++slot) CHECK(s.belt_slot(1, 1, slot) == Item::None);
    }
}

TEST_CASE("inserter waits holding an item when the belt is full", "[belts]") {
    Config c = corner_cfg();
    c.input_chest = {2, 0};
    Simulator s(make(c, {{Inserter, 2, 1, South}, {Belt, 2, 2, East}}), c);
    s.run_ticks(2000);
    CHECK(s.belt_slot(1, 1, kBeltSlots - 1) == Item::Plate);
    CHECK(s.belt_slot(1, 1, kBeltSlots / 2) == Item::Plate);
    CHECK(s.inserter_hand(0) == Item::Plate);
    const long long taken = s.counters().plates_taken_from_input;
    s.run_ticks(500);
    CHECK(s.counters().plates_taken_from_input == taken);
    CHECK(s.conservation_holds());
}

TEST_CASE("plates travel by belt into an assembler", "[belts]") {
    Config c;  // default chests (0,4) and (9,4)
    Simulator s(make(c, {{Inserter, 1, 4, East}, {Belt, 2, 4, East}, {Belt, 3, 4, East}, {Inserter, 4, 4, East},
                         {Assembler, 5, 3, North}, {Inserter, 8, 4, East}}), c);
    for (int i = 0; i < c.warmup_ticks; ++i) {
        s.tick();
        REQUIRE(s.conservation_holds());
    }
    const long long before = s.counters().gears_delivered;
    for (int i = 0; i < c.measure_ticks; ++i) {
        s.tick();
        REQUIRE(s.conservation_holds());
    }
    CHECK((s.counters().gears_delivered - before) / 60.0 == Approx(60.0 / 72 / 2).margin(0.02));
}

TEST_CASE("belt loop keeps its items", "[belts]") {
    Config c = corner_cfg();
    Simulator s(make(c, {{Belt, 1, 1, East}, {Belt, 2, 1, South}, {Belt, 2, 2, West}, {Belt, 1, 2, North}}), c);
    s.set_belt_slot(0, 0, 0, Item::Plate);
    s.set_belt_slot(1, 1, 2, Item::Plate);
    s.set_belt_slot(2, 0, 3, Item::Gear);
    for (int i = 0; i < 1000; ++i) {
        s.tick();
        REQUIRE(count_on_belts(s, 4) == 3);
    }
}

TEST_CASE("reference layout reaches the 1.25 gears/s ceiling", "[belts]") {
    Config c;  // default chests (0,4) and (9,4)
    std::vector<Entity> es = {
        // three assemblers, each fed straight from the input chest
        {Assembler, 0, 0, North}, {Assembler, 2, 3, North}, {Assembler, 0, 6, North},
        {Inserter, 0, 3, North}, {Inserter, 1, 4, East}, {Inserter, 0, 5, South},
        // top assembler -> belt along row 1 -> down column 9 -> output chest
        {Inserter, 3, 1, East},
        {Belt, 4, 1, East}, {Belt, 5, 1, East}, {Belt, 6, 1, East}, {Belt, 7, 1, East}, {Belt, 8, 1, East},
        {Belt, 9, 1, South}, {Belt, 9, 2, South}, {Inserter, 9, 3, South},
        // middle assembler -> short belt -> output chest
        {Inserter, 5, 4, East}, {Belt, 6, 4, East}, {Belt, 7, 4, East}, {Inserter, 8, 4, East},
        // bottom assembler -> belt along row 7 -> up column 9 -> output chest
        {Inserter, 3, 7, East},
        {Belt, 4, 7, East}, {Belt, 5, 7, East}, {Belt, 6, 7, East}, {Belt, 7, 7, East}, {Belt, 8, 7, East},
        {Belt, 9, 7, North}, {Belt, 9, 6, North}, {Inserter, 9, 5, North},
    };
    SimResult r = simulate(make(c, es), c);
    CHECK(r.plates_out_of_input == Approx(2.5).margin(0.05));
    CHECK(r.gears_delivered == Approx(1.25).margin(0.03));
}
