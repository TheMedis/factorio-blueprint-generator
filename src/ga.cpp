#include "ga.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numeric>
#include <thread>

namespace factory {
namespace {

int uniform_int(Rng& rng, int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
Direction random_dir(Rng& rng) { return Direction(uniform_int(rng, 0, 3)); }

EntityType random_type(Rng& rng) {
    const int r = uniform_int(rng, 0, 9);
    return r < 2 ? EntityType::Assembler : r < 6 ? EntityType::Inserter : EntityType::Belt;
}

// Tiles covered by chests or any entity (clipped to the grid; overlaps allowed).
std::vector<char> occupied_tiles(const Layout& l) {
    const int n = l.size();
    std::vector<char> occ(std::size_t(n) * n, 0);
    auto mark = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < n && y < n) occ[y * n + x] = 1;
    };
    mark(l.input_chest().x, l.input_chest().y);
    mark(l.output_chest().x, l.output_chest().y);
    for (const Entity& e : l.entities()) {
        const int s = entity_size(e.type);
        for (int dy = 0; dy < s; ++dy)
            for (int dx = 0; dx < s; ++dx) mark(e.x + dx, e.y + dy);
    }
    return occ;
}

bool fits(const std::vector<char>& occ, int n, int x, int y, int s) {
    if (x < 0 || y < 0 || x + s > n || y + s > n) return false;
    for (int dy = 0; dy < s; ++dy)
        for (int dx = 0; dx < s; ++dx)
            if (occ[(y + dy) * n + x + dx]) return false;
    return true;
}

// Index of a random inserter or belt, or -1 if there is none.
int random_small_entity(const Layout& l, Rng& rng) {
    std::vector<int> idx;
    for (int i = 0; i < int(l.entities().size()); ++i)
        if (l.entities()[i].type != EntityType::Assembler) idx.push_back(i);
    return idx.empty() ? -1 : idx[uniform_int(rng, 0, int(idx.size()) - 1)];
}

void add_random_entity(Layout& l, Rng& rng) {
    const std::vector<char> occ = occupied_tiles(l);
    const int n = l.size();
    for (int attempt = 0; attempt < 20; ++attempt) {
        const EntityType t = random_type(rng);
        const int s = entity_size(t);
        const int x = uniform_int(rng, 0, n - s), y = uniform_int(rng, 0, n - s);
        if (!fits(occ, n, x, y, s)) continue;
        l.entities().push_back({t, x, y, random_dir(rng)});
        return;
    }
}

void attach_inserter(Layout& l, Rng& rng) {
    enum class Role { Assembler, Input, Output };
    struct Target { int x, y, s; Role role; };
    std::vector<Target> targets{{l.input_chest().x, l.input_chest().y, 1, Role::Input},
                                {l.output_chest().x, l.output_chest().y, 1, Role::Output}};
    for (const Entity& e : l.entities())
        if (e.type == EntityType::Assembler) targets.push_back({e.x, e.y, 3, Role::Assembler});
    const Target t = targets[uniform_int(rng, 0, int(targets.size()) - 1)];

    const std::vector<char> occ = occupied_tiles(l);
    const int n = l.size();
    struct Spot { int x, y; Direction outward; };
    std::vector<Spot> spots;
    for (int dy = 0; dy < t.s; ++dy)
        for (int dx = 0; dx < t.s; ++dx)
            for (int d = 0; d < 4; ++d) {
                const Vec v = dir_vec(Direction(d));
                const int x = t.x + dx + v.x, y = t.y + dy + v.y;
                const bool inside = x >= t.x && x < t.x + t.s && y >= t.y && y < t.y + t.s;
                if (!inside && fits(occ, n, x, y, 1)) spots.push_back({x, y, Direction(d)});
            }
    if (spots.empty()) return;
    const Spot sp = spots[uniform_int(rng, 0, int(spots.size()) - 1)];
    // Pointing "outward" takes from the target; pointing back toward it feeds it.
    Direction dir;
    switch (t.role) {
    case Role::Input: dir = sp.outward; break;
    case Role::Output: dir = opposite(sp.outward); break;
    default: dir = uniform_int(rng, 0, 1) ? sp.outward : opposite(sp.outward); break;
    }
    l.entities().push_back({EntityType::Inserter, sp.x, sp.y, dir});
}

void belt_run(Layout& l, Rng& rng) {
    std::vector<char> occ = occupied_tiles(l);
    const int n = l.size();
    int x = uniform_int(rng, 0, n - 1), y = uniform_int(rng, 0, n - 1);
    const Direction d = random_dir(rng);
    const Vec v = dir_vec(d);
    const int len = uniform_int(rng, 2, 5);
    for (int i = 0; i < len && fits(occ, n, x, y, 1); ++i, x += v.x, y += v.y) {
        l.entities().push_back({EntityType::Belt, x, y, d});
        occ[y * n + x] = 1;
    }
}

}  // namespace

double fitness(const SimResult& r, const Config& c) {
    return c.w_gears_delivered * r.gears_delivered + c.w_gears_crafted * r.gears_crafted +
           c.w_plates_into_assemblers * r.plates_into_assemblers + c.w_plates_out_of_input * r.plates_out_of_input -
           c.w_entity_count * r.entity_count;
}

double evaluate(const Layout& layout, const Config& cfg, SimResult* out) {
    const SimResult r = simulate(layout, cfg);
    if (out) *out = r;
    return fitness(r, cfg);
}

std::vector<Layout> seed_layouts(const Config& cfg) {
    const Vec in = cfg.input_chest, out = cfg.output_chest;
    std::vector<Layout> seeds;
    if (in.y != out.y || out.x - in.x < 8) return seeds;
    // chest -> inserter -> assembler -> inserter -> belts -> inserter -> chest
    std::vector<Entity> es{{EntityType::Inserter, in.x + 1, in.y, Direction::East},
                           {EntityType::Assembler, in.x + 2, in.y - 1, Direction::North},
                           {EntityType::Inserter, in.x + 5, in.y, Direction::East}};
    for (int x = in.x + 6; x <= out.x - 2; ++x) es.push_back({EntityType::Belt, x, in.y, Direction::East});
    es.push_back({EntityType::Inserter, out.x - 1, out.y, Direction::East});
    Layout l(cfg.grid_size, in, out, std::move(es));
    if (!l.validate(cfg.max_entities)) seeds.push_back(std::move(l));
    return seeds;
}

Layout random_layout(const Config& cfg, Rng& rng) {
    Layout l = Layout::empty(cfg);
    const int n = uniform_int(rng, cfg.init_min_entities, cfg.init_max_entities);
    for (int i = 0; i < n; ++i) add_random_entity(l, rng);
    l.repair(cfg.max_entities);
    return l;
}

void apply_mutation(Layout& l, Mutation m, Rng& rng) {
    std::vector<Entity>& es = l.entities();
    switch (m) {
    case Mutation::Add: add_random_entity(l, rng); break;
    case Mutation::Remove:
        if (!es.empty()) es.erase(es.begin() + uniform_int(rng, 0, int(es.size()) - 1));
        break;
    case Mutation::Move:
        if (!es.empty()) {
            Entity& e = es[uniform_int(rng, 0, int(es.size()) - 1)];
            const Vec v = dir_vec(random_dir(rng));
            const int dist = uniform_int(rng, 1, 2);
            e.x += v.x * dist;
            e.y += v.y * dist;
        }
        break;
    case Mutation::Rotate:
        if (int i = random_small_entity(l, rng); i >= 0) es[i].dir = Direction((int(es[i].dir) + uniform_int(rng, 1, 3)) % 4);
        break;
    case Mutation::Retype:
        if (int i = random_small_entity(l, rng); i >= 0)
            es[i].type = es[i].type == EntityType::Belt ? EntityType::Inserter : EntityType::Belt;
        break;
    case Mutation::AttachInserter: attach_inserter(l, rng); break;
    case Mutation::BeltRun: belt_run(l, rng); break;
    }
}

void mutate(Layout& l, const Config& cfg, Rng& rng) {
    std::discrete_distribution<int> pick(cfg.mutation_weights.begin(), cfg.mutation_weights.end());
    const int k = uniform_int(rng, 1, 3);
    for (int i = 0; i < k; ++i) apply_mutation(l, Mutation(pick(rng)), rng);
    l.repair(cfg.max_entities);
}

Layout crossover(const Layout& a, const Layout& b, const Config& cfg, Rng& rng) {
    const bool vertical = uniform_int(rng, 0, 1) == 0;
    const int cut = uniform_int(rng, 1, a.size() - 1);
    auto from_a = [&](const Entity& e) {
        const int centre = (vertical ? e.x : e.y) + entity_size(e.type) / 2;
        return centre < cut;
    };
    Layout child(a.size(), a.input_chest(), a.output_chest());
    for (const Entity& e : a.entities())
        if (from_a(e)) child.entities().push_back(e);
    for (const Entity& e : b.entities())
        if (!from_a(e)) child.entities().push_back(e);
    child.repair(cfg.max_entities);
    return child;
}

Layout prune(const Layout& layout, const Config& cfg) {
    Layout best = layout;
    double best_fit = evaluate(best, cfg);
    for (bool improved = true; improved;) {
        improved = false;
        for (std::size_t i = 0; i < best.entities().size(); ++i) {
            Layout candidate = best;
            candidate.entities().erase(candidate.entities().begin() + i);
            const double f = evaluate(candidate, cfg);
            if (f >= best_fit) {
                best = std::move(candidate);
                best_fit = f;
                improved = true;
                break;
            }
        }
    }
    return best;
}

std::vector<double> evaluate_all(const std::vector<Layout>& pop, const Config& cfg, int threads,
                                 std::vector<SimResult>& results) {
    results.assign(pop.size(), SimResult{});
    std::vector<double> fit(pop.size(), 0.0);
    int n = threads > 0 ? threads : int(std::max(1u, std::thread::hardware_concurrency()));
    n = std::max(1, std::min(n, int(pop.size())));
    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        for (std::size_t i; (i = next.fetch_add(1)) < pop.size();) fit[i] = evaluate(pop[i], cfg, &results[i]);
    };
    std::vector<std::thread> pool;
    for (int t = 1; t < n; ++t) pool.emplace_back(worker);
    worker();
    for (std::thread& t : pool) t.join();
    return fit;
}

GaResult run_ga(const Config& cfg, const GaParams& params,
                const std::function<void(const GenerationReport&)>& on_generation) {
    Rng rng(params.seed);
    const int P = params.population;
    const std::vector<Layout> seeds = seed_layouts(cfg);
    const int seeded = seeds.empty() ? 0 : int(std::lround(cfg.seed_fraction * P));
    // The first `seeded` of each fresh batch are hand-made seeds; the rest are random.
    auto fresh = [&](int i) { return i < seeded ? seeds[i % seeds.size()] : random_layout(cfg, rng); };

    std::vector<Layout> pop;
    for (int i = 0; i < P; ++i) pop.push_back(fresh(i));
    std::vector<SimResult> results;
    std::vector<double> fit = evaluate_all(pop, cfg, params.threads, results);

    auto best_index = [&] { return int(std::max_element(fit.begin(), fit.end()) - fit.begin()); };
    auto ranked = [&] {
        std::vector<int> order(P);
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return fit[a] > fit[b]; });
        return order;
    };

    int bi = best_index();
    GaResult out{pop[bi], results[bi], fit[bi], {}};
    auto report = [&](int gen, bool improved) {
        out.best_history.push_back(out.best_fitness);
        if (!on_generation) return;
        const double mean = std::accumulate(fit.begin(), fit.end(), 0.0) / P;
        on_generation({gen, out.best_fitness, out.best_result.gears_delivered, mean, improved, &out.best});
    };
    report(0, true);

    int stagnant = 0;
    for (int gen = 1; gen <= params.generations; ++gen) {
        const std::vector<int> order = ranked();
        auto tournament = [&]() -> const Layout& {
            int best = uniform_int(rng, 0, P - 1);
            for (int k = 1; k < cfg.tournament_size; ++k) {
                const int c = uniform_int(rng, 0, P - 1);
                if (fit[c] > fit[best]) best = c;
            }
            return pop[best];
        };

        std::vector<Layout> next;
        for (int e = 0; e < cfg.elites && e < P; ++e) next.push_back(pop[order[e]]);
        while (int(next.size()) < P) {
            const Layout& a = tournament();
            const bool cross = std::uniform_real_distribution<double>(0.0, 1.0)(rng) < cfg.crossover_rate;
            Layout child = cross ? crossover(a, tournament(), cfg, rng) : a;
            mutate(child, cfg, rng);
            next.push_back(std::move(child));
        }
        pop = std::move(next);
        fit = evaluate_all(pop, cfg, params.threads, results);

        bi = best_index();
        const bool improved = fit[bi] > out.best_fitness;
        if (improved) {
            out.best = pop[bi];
            out.best_result = results[bi];
            out.best_fitness = fit[bi];
            stagnant = 0;
        } else if (++stagnant >= cfg.stagnation_generations) {
            // Replace the worst fraction with fresh seeds and random genomes to restore diversity.
            const std::vector<int> worst_last = ranked();
            const int replace = int(cfg.stagnation_replace_fraction * P);
            std::vector<Layout> batch;
            for (int i = 0; i < replace; ++i) batch.push_back(fresh(i));
            std::vector<SimResult> batch_results;
            const std::vector<double> batch_fit = evaluate_all(batch, cfg, params.threads, batch_results);
            for (int i = 0; i < replace; ++i) {
                const int dst = worst_last[P - 1 - i];
                pop[dst] = std::move(batch[i]);
                fit[dst] = batch_fit[i];
                results[dst] = batch_results[i];
            }
            stagnant = 0;
        }
        report(gen, improved);
    }

    out.best = prune(out.best, cfg);
    out.best_fitness = evaluate(out.best, cfg, &out.best_result);
    return out;
}

}  // namespace factory
