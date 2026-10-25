#pragma once

#include "layout.hpp"

#include <string>

namespace factory {

// One character per tile, one line per row, each line ending in '\n'.
std::string render_ascii(const Layout& layout);
std::string ascii_legend();
std::string render_svg(const Layout& layout);

}  // namespace factory
