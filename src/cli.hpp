#pragma once

#include "ga.hpp"

#include <string>
#include <variant>
#include <vector>

namespace factory {

struct RunOptions {
    GaParams ga;
    std::string out = "best";  // writes <out>.json and <out>.svg
    bool use_seeds = false;    // start ~10% of the population from the hand-made seed layout
};

struct SimOptions {
    std::string layout_path;
};

struct BlueprintOptions {
    std::string layout_path;
};

struct CliError {
    std::string message;
};

using CliCommand = std::variant<RunOptions, SimOptions, BlueprintOptions, CliError>;

// `args` excludes the program name.
CliCommand parse_cli(const std::vector<std::string>& args);
std::string usage();

}  // namespace factory
