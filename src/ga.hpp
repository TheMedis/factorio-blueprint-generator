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

}  // namespace factory
