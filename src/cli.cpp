#include "cli.hpp"

#include <charconv>

namespace factory {
namespace {

template <typename T>
bool parse_number(const std::string& s, T& out) {
    const char* end = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(s.data(), end, out);
    return ec == std::errc() && ptr == end;
}

CliCommand parse_run(const std::vector<std::string>& args) {
    RunOptions o;
    for (std::size_t i = 1; i < args.size(); i += 2) {
        const std::string& flag = args[i];
        if (flag == "--seeds") {
            o.use_seeds = true;
            --i;  // takes no value
            continue;
        }
        if (i + 1 >= args.size()) return CliError{"missing value for " + flag};
        const std::string& value = args[i + 1];
        const std::string bad = "value '" + value + "' for " + flag + " is not a valid number";
        if (flag == "--pop") {
            if (!parse_number(value, o.ga.population)) return CliError{bad};
        } else if (flag == "--gens") {
            if (!parse_number(value, o.ga.generations)) return CliError{bad};
        } else if (flag == "--seed") {
            if (!parse_number(value, o.ga.seed)) return CliError{bad};
        } else if (flag == "--threads") {
            if (!parse_number(value, o.ga.threads)) return CliError{bad};
        } else if (flag == "--out") {
            o.out = value;
        } else {
            return CliError{"unknown option " + flag};
        }
    }
    if (o.ga.population < 3) return CliError{"--pop must be >= 3"};
    if (o.ga.generations < 0) return CliError{"--gens must be >= 0"};
    if (o.ga.threads < 0) return CliError{"--threads must be >= 0"};
    return o;
}

}  // namespace

CliCommand parse_cli(const std::vector<std::string>& args) {
    if (args.empty()) return CliError{"missing command"};
    if (args[0] == "run") return parse_run(args);
    if (args[0] == "sim" || args[0] == "blueprint") {
        if (args.size() < 2) return CliError{args[0] + " needs a layout file"};
        if (args.size() > 2) return CliError{args[0] + " takes exactly one layout file"};
        if (args[0] == "sim") return SimOptions{args[1]};
        return BlueprintOptions{args[1]};
    }
    return CliError{"unknown command '" + args[0] + "'"};
}

std::string usage() {
    return "usage:\n"
           "  factory run [--pop 200] [--gens 500] [--seed 42] [--threads N] [--out best] [--seeds]\n"
           "  factory sim <layout.json>\n"
           "  factory blueprint <layout.json>   (prints a Factorio 2.0 blueprint string)\n";
}

}  // namespace factory
