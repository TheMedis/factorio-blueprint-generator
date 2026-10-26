#pragma once

#include "layout.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace factory {

// Factorio 2.0 blueprint for a layout: assemblers (with the gear recipe), inserters, belts and
// the output chest. The input chest tile is left empty and no power is included; the player
// places an infinity chest of iron plates and power themselves.
nlohmann::json blueprint_json(const Layout& layout);
// The importable string: '0' + base64(zlib(json)).
std::string blueprint_string(const Layout& layout);
// Inverse of blueprint_string. Throws std::runtime_error on malformed input.
nlohmann::json decode_blueprint_string(const std::string& s);

}  // namespace factory
