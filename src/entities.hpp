#pragma once

#include <array>
#include <optional>
#include <string>

namespace factory {

enum class EntityType { Assembler, Inserter, Belt };
enum class Direction { North = 0, East = 1, South = 2, West = 3 };

struct Vec {
    int x = 0;
    int y = 0;
    bool operator==(const Vec&) const = default;
};

struct Entity {
    EntityType type;
    int x;          // Assembler: top-left tile of its 3x3 footprint. Others: their tile.
    int y;
    Direction dir;  // Inserter: direction it drops toward. Belt: travel direction. Assembler: ignored.
    bool operator==(const Entity&) const = default;
};

inline constexpr int kBeltSlots = 4;  // item slots per lane per belt tile

Vec dir_vec(Direction d);
Direction opposite(Direction d);
Direction rotate_cw(Direction d);
Direction rotate_ccw(Direction d);

int entity_size(EntityType t);
const char* factorio_name(EntityType t);
std::optional<EntityType> type_from_factorio_name(const std::string& name);
const char* direction_name(Direction d);
std::optional<Direction> direction_from_name(const std::string& name);

struct Config {
    // Grid and fixed chests
    int grid_size = 10;
    Vec input_chest{0, 4};
    Vec output_chest{9, 4};
    int max_entities = 60;

    // Tier-1 timing in ticks (60 ticks = 1 s)
    int craft_ticks = 60;           // iron gear: 0.5 s recipe / crafting speed 0.5
    int plates_per_gear = 2;
    int inserter_swing_ticks = 36;  // one direction; full cycle = 72 ticks
    int belt_slot_ticks = 8;        // yellow belt: 1.875 tiles/s with 4 slots per tile
    int assembler_plate_limit = 4;  // inserters stop feeding at 2 crafts' worth
    int assembler_output_cap = 10;  // crafting pauses at this many gears in the output

    // Measurement
    int warmup_ticks = 600;
    int measure_ticks = 3600;

    // Fitness weights
    double w_gears_delivered = 100.0;
    double w_gears_crafted = 20.0;
    double w_plates_into_assemblers = 2.0;
    double w_plates_out_of_input = 1.0;
    double w_entity_count = 0.01;

    // GA
    int tournament_size = 3;
    int elites = 2;
    double crossover_rate = 0.7;
    int stagnation_generations = 50;
    double stagnation_replace_fraction = 0.2;
    double seed_fraction = 0.0;  // share of fresh genomes copied from hand-made seeds (0 = random only)
    int init_min_entities = 5;
    int init_max_entities = 25;
    // Order: Add, Remove, Move, Rotate, Retype, AttachInserter, BeltRun
    std::array<double, 7> mutation_weights{1.0, 1.0, 1.0, 1.0, 0.5, 2.0, 1.0};
};

}  // namespace factory
