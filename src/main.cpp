#include "blueprint.hpp"
#include "cli.hpp"
#include "render.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>

using namespace factory;

namespace {

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
    return bool(f);
}

void print_result(const SimResult& r) {
    std::printf("gears delivered/s %.3f | crafted/s %.3f | plates into assemblers/s %.3f | plates out/s %.3f | entities %d\n",
                r.gears_delivered, r.gears_crafted, r.plates_into_assemblers, r.plates_out_of_input, r.entity_count);
}

int run_command(const RunOptions& o, const Config& cfg) {
    const auto t0 = std::chrono::steady_clock::now();
    const GaResult r = run_ga(cfg, o.ga, [](const GenerationReport& g) {
        std::printf("gen %4d  best %8.3f  gears/s %.3f  mean %8.3f\n", g.generation, g.best_fitness,
                    g.best_gears_per_s, g.mean_fitness);
        if (g.improved) std::cout << render_ascii(*g.best);
    });
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::cout << "\nbest layout (pruned):\n" << render_ascii(r.best) << ascii_legend();
    print_result(r.best_result);
    std::printf("fitness %.3f, %.1f s\n", r.best_fitness, secs);

    const std::string json_path = o.out + ".json", svg_path = o.out + ".svg", bp_path = o.out + ".blueprint.txt";
    if (!write_file(json_path, layout_to_json(r.best).dump(2)) || !write_file(svg_path, render_svg(r.best)) ||
        !write_file(bp_path, blueprint_string(r.best) + "\n")) {
        std::cerr << "error: cannot write " << json_path << " / " << svg_path << " / " << bp_path << "\n";
        return 1;
    }
    std::cout << "wrote " << json_path << ", " << svg_path << " and " << bp_path << "\n";
    return 0;
}

// Loads and validates a layout file; prints an error and returns nullopt on failure.
std::optional<Layout> load_layout(const std::string& path, const Config& cfg) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "error: cannot open " << path << "\n";
        return std::nullopt;
    }
    try {
        return layout_from_json(nlohmann::json::parse(f), cfg.max_entities);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return std::nullopt;
    }
}

int sim_command(const SimOptions& o, const Config& cfg) {
    const std::optional<Layout> l = load_layout(o.layout_path, cfg);
    if (!l) return 1;
    std::cout << render_ascii(*l) << ascii_legend();
    print_result(simulate(*l, cfg));
    return 0;
}

int blueprint_command(const BlueprintOptions& o, const Config& cfg) {
    const std::optional<Layout> l = load_layout(o.layout_path, cfg);
    if (!l) return 1;
    if (l->size() > 16) std::cerr << "warning: grid larger than 16x16; the two substations will not power all of it\n";
    std::cout << blueprint_string(*l) << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const CliCommand cmd = parse_cli(std::vector<std::string>(argv + 1, argv + argc));
    if (const auto* err = std::get_if<CliError>(&cmd)) {
        std::cerr << "error: " << err->message << "\n\n" << usage();
        return 1;
    }
    Config cfg;
    if (const auto* run = std::get_if<RunOptions>(&cmd)) {
        if (run->use_seeds) cfg.seed_fraction = 0.1;
        return run_command(*run, cfg);
    }
    if (const auto* bp = std::get_if<BlueprintOptions>(&cmd)) return blueprint_command(*bp, cfg);
    return sim_command(std::get<SimOptions>(cmd), cfg);
}
