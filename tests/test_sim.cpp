#include "sim.hpp"

#include <algorithm>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace factory;
using enum factory::EntityType;
using enum factory::Direction;
using Catch::Approx;

namespace {
Config with_output(Vec out) {
    Config c;
    c.output_chest = out;
    return c;
}

Layout make(const Config& c, std::vector<Entity> es) {
    Layout l(c.grid_size, c.input_chest, c.output_chest, std::move(es));
    REQUIRE_FALSE(l.validate(c.max_entities).has_value());
    return l;
}

// chest(0,4) -> inserter -> assembler(2,3) -> inserter -> chest(6,4)
std::vector<Entity> assembler_line() { return {{Inserter, 1, 4, East}, {Assembler, 2, 3, North}, {Inserter, 5, 4, East}}; }

// Runs warm-up + measurement while keeping assembler `entity` topped up with plates.
SimCounters measure_topped_up(Simulator& s, const Config& c, int entity) {
    auto step = [&] { s.set_assembler_plates(entity, c.assembler_plate_limit); s.tick(); };
    for (int i = 0; i < c.warmup_ticks; ++i) step();
    SimCounters start = s.counters();
    for (int i = 0; i < c.measure_ticks; ++i) step();
    SimCounters end = s.counters();
    return {end.plates_taken_from_input - start.plates_taken_from_input,
            end.plates_into_assemblers - start.plates_into_assemblers, end.gears_crafted - start.gears_crafted,
            end.gears_delivered - start.gears_delivered, end.plates_into_output - start.plates_into_output};
}
}  // namespace

TEST_CASE("empty layout produces nothing", "[sim]") {
    Config c;
    SimResult r = simulate(Layout::empty(c), c);
    CHECK(r == SimResult{});
}

TEST_CASE("chest -> inserter -> chest moves 0.83 plates/s", "[sim]") {
    Config c = with_output({2, 4});
    SimResult r = simulate(make(c, {{Inserter, 1, 4, East}}), c);
    CHECK(r.plates_out_of_input == Approx(60.0 / 72).margin(0.01));
    CHECK(r.gears_delivered == 0.0);
    CHECK(r.entity_count == 1);
}

TEST_CASE("inserter cycle is exactly 72 ticks", "[sim]") {
    Config c = with_output({2, 4});
    Simulator s(make(c, {{Inserter, 1, 4, East}}), c);
    s.tick();
    CHECK(s.counters().plates_taken_from_input == 1);  // picks up on the first tick
    CHECK(s.inserter_hand(0) == Item::Plate);
    s.run_ticks(35);
    CHECK(s.counters().plates_into_output == 0);        // tick 36: still swinging
    s.run_ticks(1);
    CHECK(s.counters().plates_into_output == 1);        // tick 37: dropped
    s.run_ticks(35);
    CHECK(s.counters().plates_taken_from_input == 1);   // tick 72: swinging back
    s.run_ticks(1);
    CHECK(s.counters().plates_taken_from_input == 2);   // tick 73: next pickup
}

TEST_CASE("single assembler line is input-inserter limited", "[sim]") {
    Config c = with_output({6, 4});
    SimResult r = simulate(make(c, assembler_line()), c);
    CHECK(r.gears_delivered == Approx(60.0 / 72 / 2).margin(0.01));  // ~0.4167
    CHECK(r.gears_crafted == Approx(60.0 / 72 / 2).margin(0.01));
    CHECK(r.plates_into_assemblers == Approx(60.0 / 72).margin(0.01));
}

TEST_CASE("topped-up assembler: output inserter limits delivery", "[sim]") {
    Config c = with_output({6, 4});
    Simulator s(make(c, assembler_line()), c);
    SimCounters d = measure_topped_up(s, c, 1);
    CHECK(d.gears_delivered / 60.0 == Approx(60.0 / 72).margin(0.02));  // ~0.83
}

TEST_CASE("topped-up assembler with no output cap crafts 1 gear/s", "[sim]") {
    Config c = with_output({6, 4});
    c.assembler_output_cap = 1'000'000;
    Simulator s(make(c, assembler_line()), c);
    SimCounters d = measure_topped_up(s, c, 1);
    CHECK(d.gears_crafted / 60.0 == Approx(1.0).margin(0.02));
}

TEST_CASE("assembler respects the insertion limit and output cap", "[sim]") {
    Config c = with_output({6, 4});
    Simulator s(make(c, {{Inserter, 1, 4, East}, {Assembler, 2, 3, North}}), c);
    int max_plates = 0;
    for (int i = 0; i < 6000; ++i) {
        s.tick();
        max_plates = std::max(max_plates, s.assembler_plates(1));
    }
    CHECK(max_plates <= c.assembler_plate_limit + 1);  // + 1 feeding inserter
    CHECK(s.assembler_gears(1) == c.assembler_output_cap);
    CHECK(s.assembler_plates(1) == c.assembler_plate_limit);
}

TEST_CASE("inserters with nothing to do stay idle", "[sim]") {
    Config c;
    Layout l = make(c, {
        {Inserter, 1, 4, West},   // picks from an empty tile, drops into the input chest
        {Inserter, 8, 4, West},   // picks from the output chest
        {Inserter, 0, 3, North},  // picks from the input chest, drops on an empty tile
        {Inserter, 0, 5, South},  // picks from the input chest, drops into an inserter
        {Inserter, 0, 6, South},  // picks from an inserter
    });
    Simulator s(l, c);
    s.run_ticks(1000);
    CHECK(s.counters().plates_taken_from_input == 0);
    for (int i = 0; i < 5; ++i) CHECK(s.inserter_hand(i) == Item::None);
    CHECK(s.conservation_holds());
}

TEST_CASE("item conservation holds every tick", "[sim]") {
    Config c = with_output({6, 4});
    Simulator s(make(c, assembler_line()), c);
    for (int i = 0; i < 2000; ++i) {
        s.tick();
        REQUIRE(s.conservation_holds());
    }
}

TEST_CASE("simulation is deterministic", "[sim]") {
    Config c = with_output({6, 4});
    Layout l = make(c, assembler_line());
    CHECK(simulate(l, c) == simulate(l, c));
}

TEST_CASE("hooks reject entity indices of the wrong type", "[sim]") {
    Config c = with_output({6, 4});
    Simulator s(make(c, assembler_line()), c);
    CHECK_THROWS_AS(s.assembler_plates(0), std::out_of_range);  // entity 0 is an inserter
    CHECK_THROWS_AS(s.inserter_hand(1), std::out_of_range);     // entity 1 is an assembler
    CHECK_THROWS_AS(s.belt_slot(7, 0, 0), std::out_of_range);   // no such entity
}
