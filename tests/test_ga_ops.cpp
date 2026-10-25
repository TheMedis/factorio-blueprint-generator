#include "ga.hpp"

#include <algorithm>
#include <cstdlib>
#include <tuple>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace factory;
using enum factory::EntityType;
using enum factory::Direction;
using Catch::Approx;

namespace {
std::vector<Entity> sorted(std::vector<Entity> es) {
    std::sort(es.begin(), es.end(), [](const Entity& a, const Entity& b) {
        return std::tuple(int(a.type), a.x, a.y, int(a.dir)) < std::tuple(int(b.type), b.x, b.y, int(b.dir));
    });
    return es;
}

int count_type(const Layout& l, EntityType t) {
    return int(std::count_if(l.entities().begin(), l.entities().end(), [t](const Entity& e) { return e.type == t; }));
}
}  // namespace

TEST_CASE("fitness formula", "[ga]") {
    Config c;
    SimResult r{1.0, 1.0, 2.0, 2.5, 10};
    CHECK(fitness(r, c) == Approx(100.0 + 20.0 + 4.0 + 2.5 - 0.1));
    SimResult best_partial{0.0, 1.25, 2.5, 2.5, 0};  // maximum partial credit without delivery
    CHECK(fitness(SimResult{1.0, 0, 0, 0, 0}, c) > fitness(best_partial, c));
}

TEST_CASE("seed layout is valid and produces gears", "[ga]") {
    Config c;
    std::vector<Layout> seeds = seed_layouts(c);
    REQUIRE(seeds.size() == 1);
    CHECK_FALSE(seeds[0].validate(c.max_entities).has_value());
    SimResult r;
    evaluate(seeds[0], c, &r);
    CHECK(r.gears_delivered == Approx(60.0 / 72 / 2).margin(0.02));
}

TEST_CASE("random layouts are valid", "[ga]") {
    Config c;
    Rng rng(1);
    for (int i = 0; i < 500; ++i) {
        Layout l = random_layout(c, rng);
        REQUIRE_FALSE(l.validate(c.max_entities).has_value());
        REQUIRE(int(l.entities().size()) <= c.init_max_entities);
    }
}

TEST_CASE("individual mutations do what they say", "[ga]") {
    Config c;
    Rng rng(5);
    const Layout seed = seed_layouts(c).at(0);

    Layout added = Layout::empty(c);
    apply_mutation(added, Mutation::Add, rng);
    CHECK(added.entities().size() == 1);

    Layout removed = seed;
    apply_mutation(removed, Mutation::Remove, rng);
    CHECK(removed.entities().size() == seed.entities().size() - 1);

    Layout retyped = seed;
    apply_mutation(retyped, Mutation::Retype, rng);
    CHECK(std::abs(count_type(retyped, Belt) - count_type(seed, Belt)) == 1);
    CHECK(count_type(retyped, Assembler) == 1);

    Layout rotated = seed;
    apply_mutation(rotated, Mutation::Rotate, rng);
    CHECK(rotated.entities() != seed.entities());
    CHECK(rotated.entities().size() == seed.entities().size());

    for (int i = 0; i < 100; ++i) {
        Layout run = Layout::empty(c);
        apply_mutation(run, Mutation::BeltRun, rng);
        REQUIRE(run.entities().size() <= 5);
        for (const Entity& e : run.entities()) {
            REQUIRE(e.type == Belt);
            REQUIRE(e.dir == run.entities()[0].dir);
        }
        REQUIRE_FALSE(run.validate(c.max_entities).has_value());
    }

    for (int i = 0; i < 100; ++i) {
        Layout attached = seed;
        apply_mutation(attached, Mutation::AttachInserter, rng);
        REQUIRE(attached.entities().size() <= seed.entities().size() + 1);
        REQUIRE_FALSE(attached.validate(c.max_entities).has_value());
        if (attached.entities().size() == seed.entities().size()) continue;
        const Entity& ins = attached.entities().back();
        REQUIRE(ins.type == Inserter);
        // An inserter next to the input chest always points away from it.
        const Vec back{ins.x - dir_vec(ins.dir).x, ins.y - dir_vec(ins.dir).y};
        const Vec front{ins.x + dir_vec(ins.dir).x, ins.y + dir_vec(ins.dir).y};
        REQUIRE_FALSE(front == c.input_chest);
        REQUIRE_FALSE(back == c.output_chest);
    }
}

TEST_CASE("mutate and crossover always yield valid layouts", "[ga]") {
    Config c;
    Rng rng(7);
    std::vector<Layout> pool = seed_layouts(c);
    while (pool.size() < 20) pool.push_back(random_layout(c, rng));
    std::uniform_int_distribution<int> pick(0, int(pool.size()) - 1);
    for (int i = 0; i < 10000; ++i) {
        const Layout& a = pool[pick(rng)];
        Layout child = i % 2 == 0 ? crossover(a, pool[pick(rng)], c, rng) : a;
        mutate(child, c, rng);
        REQUIRE_FALSE(child.validate(c.max_entities).has_value());
        pool[pick(rng)] = std::move(child);
    }
}

TEST_CASE("crossover of a layout with itself keeps the same entities", "[ga]") {
    Config c;
    Rng rng(11);
    const Layout seed = seed_layouts(c).at(0);
    for (int i = 0; i < 50; ++i) CHECK(sorted(crossover(seed, seed, c, rng).entities()) == sorted(seed.entities()));
}

TEST_CASE("crossover children only contain parent entities", "[ga]") {
    Config c;
    Rng rng(13);
    Layout a = random_layout(c, rng), b = random_layout(c, rng);
    for (int i = 0; i < 200; ++i) {
        Layout child = crossover(a, b, c, rng);
        for (const Entity& e : child.entities()) {
            const bool in_a = std::find(a.entities().begin(), a.entities().end(), e) != a.entities().end();
            const bool in_b = std::find(b.entities().begin(), b.entities().end(), e) != b.entities().end();
            REQUIRE((in_a || in_b));
        }
    }
}

TEST_CASE("prune removes entities that do not contribute", "[ga]") {
    Config c;
    const Layout seed = seed_layouts(c).at(0);
    Layout messy = seed;
    messy.entities().push_back({Belt, 5, 8, West});
    messy.entities().push_back({Inserter, 7, 0, North});
    REQUIRE_FALSE(messy.validate(c.max_entities).has_value());
    CHECK(sorted(prune(messy, c).entities()) == sorted(seed.entities()));
}
