#include "render.hpp"

#include <sstream>
#include <vector>

namespace factory {

std::string render_ascii(const Layout& l) {
    const int n = l.size();
    std::vector<std::string> rows(n, std::string(n, '.'));
    rows[l.input_chest().y][l.input_chest().x] = 'I';
    rows[l.output_chest().y][l.output_chest().x] = 'O';
    for (const Entity& e : l.entities()) {
        switch (e.type) {
        case EntityType::Assembler:
            for (int dy = 0; dy < 3; ++dy)
                for (int dx = 0; dx < 3; ++dx) rows[e.y + dy][e.x + dx] = 'A';
            break;
        case EntityType::Belt: rows[e.y][e.x] = "^>v<"[int(e.dir)]; break;
        case EntityType::Inserter: rows[e.y][e.x] = "nesw"[int(e.dir)]; break;
        }
    }
    std::string out;
    for (const std::string& r : rows) out += r + '\n';
    return out;
}

std::string ascii_legend() {
    return "I input chest  O output chest  A assembler  belts ^ > v <  inserters n e s w (drop direction)\n";
}

std::string render_svg(const Layout& l) {
    constexpr int T = 40;  // pixels per tile
    const int px = l.size() * T;
    static const char* arrows[] = {"↑", "→", "↓", "←"};
    std::ostringstream o;
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << px << "\" height=\"" << px << "\" viewBox=\"0 0 "
      << px << ' ' << px << "\">\n";
    o << "<rect width=\"100%\" height=\"100%\" fill=\"#1e1e1e\"/>\n";
    for (int i = 0; i <= l.size(); ++i) {
        o << "<line x1=\"" << i * T << "\" y1=\"0\" x2=\"" << i * T << "\" y2=\"" << px << "\" stroke=\"#333\"/>\n";
        o << "<line x1=\"0\" y1=\"" << i * T << "\" x2=\"" << px << "\" y2=\"" << i * T << "\" stroke=\"#333\"/>\n";
    }
    auto tile = [&](const char* cls, int x, int y, int s, const char* fill, const char* label) {
        o << "<g class=\"" << cls << "\"><rect x=\"" << x * T + 1 << "\" y=\"" << y * T + 1 << "\" width=\""
          << s * T - 2 << "\" height=\"" << s * T - 2 << "\" rx=\"4\" fill=\"" << fill << "\"/>"
          << "<text x=\"" << x * T + s * T / 2 << "\" y=\"" << y * T + s * T / 2 + (s == 3 ? 10 : 7)
          << "\" font-size=\"" << (s == 3 ? 30 : 20)
          << "\" text-anchor=\"middle\" font-family=\"monospace\" fill=\"#111\">" << label << "</text></g>\n";
    };
    tile("chest", l.input_chest().x, l.input_chest().y, 1, "#b5703a", "I");
    tile("chest", l.output_chest().x, l.output_chest().y, 1, "#b5703a", "O");
    for (const Entity& e : l.entities()) {
        switch (e.type) {
        case EntityType::Assembler: tile("assembler", e.x, e.y, 3, "#6a8caf", "⚙"); break;
        case EntityType::Belt: tile("belt", e.x, e.y, 1, "#9a9a9a", arrows[int(e.dir)]); break;
        case EntityType::Inserter: tile("inserter", e.x, e.y, 1, "#e8c547", arrows[int(e.dir)]); break;
        }
    }
    o << "</svg>\n";
    return o.str();
}

}  // namespace factory
