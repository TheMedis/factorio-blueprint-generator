#include "cli.hpp"
#include "render.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>

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

    const std::string json_path = o.out + ".json", svg_path = o.out + ".svg";
    if (!write_file(json_path, layout_to_json(r.best).dump(2)) || !write_file(svg_path, render_svg(r.best))) {
        std::cerr << "error: cannot write " << json_path << " / " << svg_path << "\n";
        return 1;
    }
    std::cout << "wrote " << json_path << " and " << svg_path << "\n";
    return 0;
}

int sim_command(const SimOptions& o, const Config& cfg) {
    std::ifstream f(o.layout_path);
    if (!f) {
        std::cerr << "error: cannot open " << o.layout_path << "\n";
        return 1;
    }
    try {
        const Layout l = layout_from_json(nlohmann::json::parse(f), cfg.max_entities);
        std::cout << render_ascii(l) << ascii_legend();
        print_result(simulate(l, cfg));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
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
    return sim_command(std::get<SimOptions>(cmd), cfg);
}
