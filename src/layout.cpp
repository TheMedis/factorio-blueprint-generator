#include "layout.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace factory {
namespace {

bool in_grid(Vec p, int size) { return p.x >= 0 && p.y >= 0 && p.x < size && p.y < size; }

std::vector<int> chest_grid(int size, Vec in, Vec out) {
    std::vector<int> occ(std::size_t(size) * size, kEmpty);
    if (in_grid(in, size)) occ[in.y * size + in.x] = kInputChest;
    if (in_grid(out, size)) occ[out.y * size + out.x] = kOutputChest;
    return occ;
}

// Marks e's footprint with `id` if it fits. Otherwise leaves `occ` untouched, returns
// false and sets *hit to the occupant it collided with (kEmpty when off-grid).
bool try_place(std::vector<int>& occ, int size, const Entity& e, int id, int* hit) {
    const int s = entity_size(e.type);
    *hit = kEmpty;
    if (e.x < 0 || e.y < 0 || e.x + s > size || e.y + s > size) return false;
    for (int dy = 0; dy < s; ++dy)
        for (int dx = 0; dx < s; ++dx)
            if (int c = occ[(e.y + dy) * size + e.x + dx]; c != kEmpty) {
                *hit = c;
                return false;
            }
    for (int dy = 0; dy < s; ++dy)
        for (int dx = 0; dx < s; ++dx) occ[(e.y + dy) * size + e.x + dx] = id;
    return true;
}

std::string describe(int index, const Entity& e) {
    return "entity " + std::to_string(index) + " (" + factorio_name(e.type) + " at " + std::to_string(e.x) +
           "," + std::to_string(e.y) + ")";
}

std::string describe_occupant(int c) {
    if (c == kInputChest) return "the input chest";
    if (c == kOutputChest) return "the output chest";
    return "entity " + std::to_string(c);
}

// Converts a Factorio centre position to the entity's top-left tile.
Vec tile_of(double cx, double cy, int s, const std::string& where) {
    const double x = cx - s / 2.0;
    const double y = cy - s / 2.0;
    const long rx = std::lround(x);
    const long ry = std::lround(y);
    if (std::abs(x - rx) > 1e-6 || std::abs(y - ry) > 1e-6)
        throw std::runtime_error(where + ": position is not aligned to the tile grid");
    return {int(rx), int(ry)};
}

}  // namespace

Layout::Layout(int size, Vec input_chest, Vec output_chest, std::vector<Entity> entities)
    : size_(size), input_(input_chest), output_(output_chest), entities_(std::move(entities)) {}

Layout Layout::empty(const Config& cfg) { return Layout(cfg.grid_size, cfg.input_chest, cfg.output_chest); }

std::optional<std::string> Layout::validate(int max_entities) const {
    if (!in_grid(input_, size_) || !in_grid(output_, size_)) return "a chest is outside the grid";
    if (input_ == output_) return "input and output chest share a tile";
    if (int(entities_.size()) > max_entities)
        return "too many entities (" + std::to_string(entities_.size()) + " > " + std::to_string(max_entities) + ")";
    std::vector<int> occ = chest_grid(size_, input_, output_);
    for (int i = 0; i < int(entities_.size()); ++i) {
        int hit;
        if (try_place(occ, size_, entities_[i], i, &hit)) continue;
        if (hit == kEmpty) return describe(i, entities_[i]) + " is outside the grid";
        return describe(i, entities_[i]) + " overlaps " + describe_occupant(hit);
    }
    return std::nullopt;
}

void Layout::repair(int max_entities) {
    std::vector<int> occ = chest_grid(size_, input_, output_);
    std::vector<Entity> kept;
    for (const Entity& e : entities_) {
        if (int(kept.size()) >= max_entities) break;
        int hit;
        if (try_place(occ, size_, e, int(kept.size()), &hit)) kept.push_back(e);
    }
    entities_ = std::move(kept);
}

std::vector<int> Layout::occupancy() const {
    if (auto err = validate(std::numeric_limits<int>::max())) throw std::logic_error("occupancy of invalid layout: " + *err);
    std::vector<int> occ = chest_grid(size_, input_, output_);
    for (int i = 0; i < int(entities_.size()); ++i) {
        int hit;
        try_place(occ, size_, entities_[i], i, &hit);
    }
    return occ;
}

nlohmann::json layout_to_json(const Layout& l) {
    using nlohmann::json;
    auto pos = [](int x, int y, int s) { return json{{"x", x + s / 2.0}, {"y", y + s / 2.0}}; };
    json entities = json::array();
    int number = 1;
    entities.push_back({{"entity_number", number++}, {"name", "wooden-chest"}, {"role", "input"},
                        {"position", pos(l.input_chest().x, l.input_chest().y, 1)}});
    entities.push_back({{"entity_number", number++}, {"name", "wooden-chest"}, {"role", "output"},
                        {"position", pos(l.output_chest().x, l.output_chest().y, 1)}});
    for (const Entity& e : l.entities()) {
        json j{{"entity_number", number++}, {"name", factorio_name(e.type)},
               {"position", pos(e.x, e.y, entity_size(e.type))}};
        if (e.type != EntityType::Assembler) j["direction"] = direction_name(e.dir);
        entities.push_back(j);
    }
    return json{{"format", "factory-layout-v1"},
                {"direction_convention",
                 "internal: inserter = drop direction, belt = travel direction (not Factorio blueprint numbering)"},
                {"grid_size", l.size()},
                {"entities", entities}};
}

Layout layout_from_json(const nlohmann::json& j, int max_entities) {
    try {
        if (j.at("format").get<std::string>() != "factory-layout-v1")
            throw std::runtime_error("unsupported layout format");
        const int size = j.at("grid_size").get<int>();
        std::optional<Vec> in, out;
        std::vector<Entity> es;
        const auto& list = j.at("entities");
        for (std::size_t i = 0; i < list.size(); ++i) {
            const auto& je = list[i];
            const std::string where = "entity #" + std::to_string(i + 1);
            const std::string name = je.at("name").get<std::string>();
            const double px = je.at("position").at("x").get<double>();
            const double py = je.at("position").at("y").get<double>();
            if (name == "wooden-chest") {
                const Vec p = tile_of(px, py, 1, where);
                const std::string role = je.at("role").get<std::string>();
                std::optional<Vec>& slot = role == "input" ? in : out;
                if (role != "input" && role != "output")
                    throw std::runtime_error(where + ": chest role must be 'input' or 'output'");
                if (slot) throw std::runtime_error(where + ": duplicate " + role + " chest");
                slot = p;
                continue;
            }
            const auto type = type_from_factorio_name(name);
            if (!type) throw std::runtime_error(where + ": unknown entity name '" + name + "'");
            const Vec p = tile_of(px, py, entity_size(*type), where);
            Direction dir = Direction::North;
            if (*type != EntityType::Assembler) {
                const auto d = direction_from_name(je.at("direction").get<std::string>());
                if (!d) throw std::runtime_error(where + ": unknown direction");
                dir = *d;
            }
            es.push_back({*type, p.x, p.y, dir});
        }
        if (!in) throw std::runtime_error("layout has no input chest");
        if (!out) throw std::runtime_error("layout has no output chest");
        Layout l(size, *in, *out, std::move(es));
        if (auto err = l.validate(max_entities)) throw std::runtime_error("invalid layout: " + *err);
        return l;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("malformed layout JSON: ") + e.what());
    }
}

}  // namespace factory
