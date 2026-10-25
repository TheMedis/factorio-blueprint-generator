#pragma once

#include "entities.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace factory {

inline constexpr int kEmpty = -1;
inline constexpr int kInputChest = -2;
inline constexpr int kOutputChest = -3;

class Layout {
public:
    Layout(int size, Vec input_chest, Vec output_chest, std::vector<Entity> entities = {});
    static Layout empty(const Config& cfg);

    int size() const { return size_; }
    Vec input_chest() const { return input_; }
    Vec output_chest() const { return output_; }
    const std::vector<Entity>& entities() const { return entities_; }
    std::vector<Entity>& entities() { return entities_; }

    // nullopt if valid; otherwise a message naming the offending entity.
    std::optional<std::string> validate(int max_entities) const;
    // Drops entities that leave the grid or overlap an earlier entity or a chest
    // (earlier entities win), then truncates to max_entities.
    void repair(int max_entities);
    // size*size cells holding kEmpty, kInputChest, kOutputChest or an entity index.
    // Throws std::logic_error if the layout is invalid.
    std::vector<int> occupancy() const;

    bool operator==(const Layout&) const = default;

private:
    int size_;
    Vec input_;
    Vec output_;
    std::vector<Entity> entities_;
};

nlohmann::json layout_to_json(const Layout& layout);
// Throws std::runtime_error with a readable message on malformed or invalid input.
Layout layout_from_json(const nlohmann::json& j, int max_entities);

}  // namespace factory
