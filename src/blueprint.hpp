#pragma once

#include "layout.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace factory {

// Factorio 2.0 blueprint for a layout, ready for editor/sandbox testing: the input chest
// becomes an infinity chest stocked with iron plates, and substations plus electric energy
// interfaces outside the grid power everything. Covers grids up to 16x16.
nlohmann::json blueprint_json(const Layout& layout);
// The importable string: '0' + base64(zlib(json)).
std::string blueprint_string(const Layout& layout);
// Inverse of blueprint_string. Throws std::runtime_error on malformed input.
nlohmann::json decode_blueprint_string(const std::string& s);

}  // namespace factory
