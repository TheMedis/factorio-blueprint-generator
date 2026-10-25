#include "ga.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace factory;

namespace {
// Shorter simulations keep Debug + sanitizer test runs fast.
Config fast_cfg() {
    Config c;
    c.warmup_ticks = 120;
    c.measure_ticks = 720;
    c.stagnation_generations = 5;  // exercise the stagnation kick
    return c;
}
}  // namespace

TEST_CASE("evaluate_all is thread-count independent", "[ga_run]") {
    Config c = fast_cfg();
    Rng rng(3);
    std::vector<Layout> pop = seed_layouts(c);
    while (pop.size() < 8) pop.push_back(random_layout(c, rng));
    std::vector<SimResult> r1, r4;
    std::vector<double> f1 = evaluate_all(pop, c, 1, r1);
    std::vector<double> f4 = evaluate_all(pop, c, 4, r4);
    CHECK(f1 == f4);
    CHECK(r1 == r4);
    for (std::size_t i = 0; i < pop.size(); ++i) CHECK(f1[i] == evaluate(pop[i], c));
}

TEST_CASE("run_ga is monotone and reproducible across thread counts", "[ga_run]") {
    Config c = fast_cfg();
    std::vector<double> reported;
    GaParams p{.population = 20, .generations = 15, .seed = 3, .threads = 2};
    GaResult a = run_ga(c, p, [&](const GenerationReport& g) {
        reported.push_back(g.best_fitness);
        REQUIRE(g.best != nullptr);
    });
    REQUIRE(reported.size() == 16);  // generation 0 plus 15
    CHECK(a.best_history == reported);
    for (std::size_t i = 1; i < reported.size(); ++i) CHECK(reported[i] >= reported[i - 1]);
    CHECK_FALSE(a.best.validate(c.max_entities).has_value());
    CHECK(a.best_fitness >= reported.back());  // pruning never lowers fitness

    p.threads = 1;
    GaResult b = run_ga(c, p);
    CHECK(a.best == b.best);
    CHECK(a.best_fitness == b.best_fitness);
}

TEST_CASE("zero generations returns the best initial genome", "[ga_run]") {
    Config c = fast_cfg();
    GaResult r = run_ga(c, GaParams{.population = 5, .generations = 0, .seed = 1, .threads = 1});
    CHECK(r.best_history.size() == 1);
    CHECK(r.best_result.gears_delivered > 0.3);  // the seed layout is in the initial population
}
