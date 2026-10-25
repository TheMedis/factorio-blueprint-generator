#pragma once

#include "layout.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace factory {

enum class Item : std::uint8_t { None, Plate, Gear };

// Rates per second over the measured window.
struct SimResult {
    double gears_delivered = 0.0;
    double gears_crafted = 0.0;
    double plates_into_assemblers = 0.0;
    double plates_out_of_input = 0.0;
    int entity_count = 0;
    bool operator==(const SimResult&) const = default;
};

// Cumulative counts since tick 0.
struct SimCounters {
    long long plates_taken_from_input = 0;
    long long plates_into_assemblers = 0;
    long long gears_crafted = 0;
    long long gears_delivered = 0;
    long long plates_into_output = 0;
};

class Simulator {
public:
    // Precondition: layout.validate(...) succeeded.
    Simulator(const Layout& layout, const Config& cfg);

    void tick();
    void run_ticks(int n);
    SimResult run();  // cfg.warmup_ticks, then cfg.measure_ticks measured

    const SimCounters& counters() const { return counters_; }
    long long tick_count() const { return tick_; }
    bool conservation_holds() const;

    // Inspection and test hooks; `entity` is an index into layout.entities().
    int assembler_plates(int entity) const;
    int assembler_gears(int entity) const;
    Item inserter_hand(int entity) const;
    Item belt_slot(int entity, int lane, int slot) const;
    // Mutating hooks create or destroy items, so they disable the per-tick conservation assert.
    void set_assembler_plates(int entity, int plates);
    void set_belt_slot(int entity, int lane, int slot, Item item);

private:
    enum class EndKind : std::uint8_t { None, InputChest, OutputChest, Assembler, Belt };
    struct Endpoint {
        EndKind kind = EndKind::None;
        int index = -1;  // index into assemblers_ or belts_
    };
    enum class Want : std::uint8_t { Nothing, Plate, Any };
    enum class Phase : std::uint8_t { WaitPickup, SwingOut, WaitDrop, SwingBack };
    enum class Join : std::uint8_t { None, Continue, SideLoad };  // Continue = straight or curve, lanes kept

    struct Assembler {
        int plates = 0;
        int gears = 0;      // output buffer
        int remaining = 0;  // ticks left in the current craft; 0 = idle
    };
    struct Inserter {
        Endpoint from;
        Endpoint to;
        int drop_lane = 1;  // lane used when `to` is a belt
        Phase phase = Phase::WaitPickup;
        int timer = 0;
        Item hand = Item::None;
    };
    struct Belt {
        // Lane 0 = left, lane 1 = right (relative to travel). Slot kBeltSlots-1 is the downstream end.
        std::array<std::array<Item, kBeltSlots>, 2> slots{};
        std::array<std::array<std::uint32_t, kBeltSlots>, 2> moved{};  // belt step that last moved into this slot
        int next = -1;  // belt this one feeds, or -1
        Join join = Join::None;
        int side_lane = 0;  // target lane when join == SideLoad
    };

    static bool accepts(Item it, Want want);
    void compile(const Layout& layout);
    void compile_belts();
    Endpoint endpoint_at(Vec p) const;
    void update_inserter(Inserter& ins);
    void update_assembler(Assembler& a);
    void step_belts();
    Want wanted_by(const Endpoint& to) const;
    Item take_from(const Endpoint& from, Want want);
    bool put_into(const Inserter& ins, Item item);
    int local(int entity, EntityType type) const;

    Config cfg_;
    int size_ = 0;
    std::vector<int> occupancy_;
    std::vector<Entity> entities_;
    std::vector<int> local_index_;  // entity index -> index within its type's vector
    std::vector<Assembler> assemblers_;
    std::vector<Inserter> inserters_;
    std::vector<Belt> belts_;
    std::vector<int> belt_order_;  // downstream first
    SimCounters counters_;
    long long tick_ = 0;
    std::uint32_t belt_stamp_ = 0;
    bool checks_enabled_ = true;
};

SimResult simulate(const Layout& layout, const Config& cfg);

}  // namespace factory
