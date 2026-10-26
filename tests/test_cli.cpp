#include "cli.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace factory;
using Catch::Matchers::ContainsSubstring;

namespace {
std::string error_of(const std::vector<std::string>& args) {
    CliCommand cmd = parse_cli(args);
    const auto* err = std::get_if<CliError>(&cmd);
    return err ? err->message : "";
}
}  // namespace

TEST_CASE("parse run with defaults", "[cli]") {
    CliCommand cmd = parse_cli({"run"});
    const auto* r = std::get_if<RunOptions>(&cmd);
    REQUIRE(r != nullptr);
    CHECK(r->ga.population == 200);
    CHECK(r->ga.generations == 500);
    CHECK(r->ga.seed == 42);
    CHECK(r->ga.threads == 0);
    CHECK(r->out == "best");
    CHECK_FALSE(r->use_seeds);
}

TEST_CASE("parse run with --seeds", "[cli]") {
    CliCommand cmd = parse_cli({"run", "--seeds", "--gens", "3"});
    const auto* r = std::get_if<RunOptions>(&cmd);
    REQUIRE(r != nullptr);
    CHECK(r->use_seeds);
    CHECK(r->ga.generations == 3);
}

TEST_CASE("parse run with flags", "[cli]") {
    CliCommand cmd = parse_cli({"run", "--pop", "50", "--gens", "0", "--seed", "7", "--threads", "2", "--out", "x/y"});
    const auto* r = std::get_if<RunOptions>(&cmd);
    REQUIRE(r != nullptr);
    CHECK(r->ga.population == 50);
    CHECK(r->ga.generations == 0);
    CHECK(r->ga.seed == 7);
    CHECK(r->ga.threads == 2);
    CHECK(r->out == "x/y");
}

TEST_CASE("parse sim", "[cli]") {
    CliCommand cmd = parse_cli({"sim", "layout.json"});
    const auto* s = std::get_if<SimOptions>(&cmd);
    REQUIRE(s != nullptr);
    CHECK(s->layout_path == "layout.json");
}

TEST_CASE("parse errors", "[cli]") {
    CHECK_THAT(error_of({}), ContainsSubstring("missing command"));
    CHECK_THAT(error_of({"fly"}), ContainsSubstring("unknown command"));
    CHECK_THAT(error_of({"run", "--pop"}), ContainsSubstring("missing value"));
    CHECK_THAT(error_of({"run", "--pop", "abc"}), ContainsSubstring("not a valid number"));
    CHECK_THAT(error_of({"run", "--pop", "12x"}), ContainsSubstring("not a valid number"));
    CHECK_THAT(error_of({"run", "--pop", "2"}), ContainsSubstring("--pop must be >= 3"));
    CHECK_THAT(error_of({"run", "--gens", "-1"}), ContainsSubstring("--gens must be >= 0"));
    CHECK_THAT(error_of({"run", "--threads", "-2"}), ContainsSubstring("--threads must be >= 0"));
    CHECK_THAT(error_of({"run", "--seed", "-5"}), ContainsSubstring("not a valid number"));
    CHECK_THAT(error_of({"run", "--bogus", "1"}), ContainsSubstring("unknown option"));
    CHECK_THAT(error_of({"sim"}), ContainsSubstring("layout file"));
    CHECK_THAT(error_of({"sim", "a.json", "b.json"}), ContainsSubstring("exactly one"));
}
