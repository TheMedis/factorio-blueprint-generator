#pragma once

#include "layout.hpp"
#include "sim.hpp"

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

namespace factory {

using Rng = std::mt19937_64;

// Same order as Config::mutation_weights.
enum class Mutation { Add, Remove, Move, Rotate, Retype, AttachInserter, BeltRun };

double fitness(const SimResult& r, const Config& cfg);
double evaluate(const Layout& layout, const Config& cfg, SimResult* out = nullptr);

// Hand-made valid starting layouts for cfg's chest positions (may be empty).
std::vector<Layout> seed_layouts(const Config& cfg);
Layout random_layout(const Config& cfg, Rng& rng);

// Applies one mutation; the result may be invalid until repair().
void apply_mutation(Layout& layout, Mutation m, Rng& rng);
// Applies 1-3 weighted mutations, then repairs.
void mutate(Layout& layout, const Config& cfg, Rng& rng);
// Spatial one-cut crossover; the result is repaired.
Layout crossover(const Layout& a, const Layout& b, const Config& cfg, Rng& rng);
// Repeatedly removes any entity whose removal does not lower fitness.
Layout prune(const Layout& layout, const Config& cfg);

struct GaParams {
    int population = 200;
    int generations = 500;
    std::uint64_t seed = 42;
    int threads = 0;  // 0 = std::thread::hardware_concurrency()
};

struct GenerationReport {
    int generation;
    double best_fitness;
    double best_gears_per_s;
    double mean_fitness;
    bool improved;
    const Layout* best;
};

struct GaResult {
    Layout best;
    SimResult best_result;
    double best_fitness;
    std::vector<double> best_history;  // best fitness after each generation, starting with generation 0
};

// Simulates every layout using `threads` workers; results[i] and the returned fitness[i] match pop[i].
std::vector<double> evaluate_all(const std::vector<Layout>& pop, const Config& cfg, int threads,
                                 std::vector<SimResult>& results);

GaResult run_ga(const Config& cfg, const GaParams& params,
                const std::function<void(const GenerationReport&)>& on_generation = {});

}  // namespace factory
