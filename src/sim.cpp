#include "sim.hpp"

#include <cassert>
#include <stdexcept>

namespace factory {

bool Simulator::accepts(Item it, Want want) {
    if (it == Item::None || want == Want::Nothing) return false;
    return want == Want::Any || it == Item::Plate;
}

Simulator::Simulator(const Layout& layout, const Config& cfg) : cfg_(cfg) { compile(layout); }

void Simulator::compile(const Layout& layout) {
    size_ = layout.size();
    entities_ = layout.entities();
    occupancy_ = layout.occupancy();
    local_index_.assign(entities_.size(), -1);
    for (std::size_t i = 0; i < entities_.size(); ++i) {
        switch (entities_[i].type) {
        case EntityType::Assembler: local_index_[i] = int(assemblers_.size()); assemblers_.emplace_back(); break;
        case EntityType::Inserter: local_index_[i] = int(inserters_.size()); inserters_.emplace_back(); break;
        case EntityType::Belt: local_index_[i] = int(belts_.size()); belts_.emplace_back(); break;
        }
    }
    for (std::size_t i = 0; i < entities_.size(); ++i) {
        const Entity& e = entities_[i];
        if (e.type != EntityType::Inserter) continue;
        Inserter& ins = inserters_[local_index_[i]];
        const Vec d = dir_vec(e.dir);
        ins.from = endpoint_at({e.x - d.x, e.y - d.y});
        ins.to = endpoint_at({e.x + d.x, e.y + d.y});
    }
    compile_belts();
}

void Simulator::compile_belts() {}  // replaced in Task 4

Simulator::Endpoint Simulator::endpoint_at(Vec p) const {
    if (p.x < 0 || p.y < 0 || p.x >= size_ || p.y >= size_) return {};
    const int o = occupancy_[p.y * size_ + p.x];
    if (o == kInputChest) return {EndKind::InputChest, -1};
    if (o == kOutputChest) return {EndKind::OutputChest, -1};
    if (o < 0) return {};
    switch (entities_[o].type) {
    case EntityType::Assembler: return {EndKind::Assembler, local_index_[o]};
    case EntityType::Belt: return {EndKind::Belt, local_index_[o]};
    case EntityType::Inserter: return {};
    }
    return {};
}

void Simulator::tick() {
    ++tick_;
    for (Inserter& ins : inserters_) update_inserter(ins);
    for (Assembler& a : assemblers_) update_assembler(a);
#ifndef NDEBUG
    if (checks_enabled_) assert(conservation_holds());
#endif
}

void Simulator::run_ticks(int n) {
    for (int i = 0; i < n; ++i) tick();
}

void Simulator::update_inserter(Inserter& ins) {
    switch (ins.phase) {
    case Phase::WaitPickup:
    case Phase::WaitDrop: break;
    case Phase::SwingOut:
        if (--ins.timer > 0) return;
        ins.phase = Phase::WaitDrop;
        break;
    case Phase::SwingBack:
        if (--ins.timer > 0) return;
        ins.phase = Phase::WaitPickup;
        break;
    }
    if (ins.phase == Phase::WaitDrop) {
        if (put_into(ins, ins.hand)) {
            ins.hand = Item::None;
            ins.phase = Phase::SwingBack;
            ins.timer = cfg_.inserter_swing_ticks;
        }
        return;
    }
    // WaitPickup: only pick up what the target can accept right now.
    const Want want = wanted_by(ins.to);
    if (want == Want::Nothing) return;
    const Item got = take_from(ins.from, want);
    if (got == Item::None) return;
    ins.hand = got;
    ins.phase = Phase::SwingOut;
    ins.timer = cfg_.inserter_swing_ticks;
}

void Simulator::update_assembler(Assembler& a) {
    if (a.remaining > 0 && --a.remaining == 0) {
        ++a.gears;
        ++counters_.gears_crafted;
    }
    if (a.remaining == 0 && a.plates >= cfg_.plates_per_gear && a.gears < cfg_.assembler_output_cap) {
        a.plates -= cfg_.plates_per_gear;  // ingredients are consumed when the craft starts
        a.remaining = cfg_.craft_ticks;
    }
}

void Simulator::step_belts() {}  // replaced in Task 4

Simulator::Want Simulator::wanted_by(const Endpoint& to) const {
    switch (to.kind) {
    case EndKind::Assembler:
        return assemblers_[to.index].plates < cfg_.assembler_plate_limit ? Want::Plate : Want::Nothing;
    case EndKind::OutputChest:
    case EndKind::Belt: return Want::Any;
    default: return Want::Nothing;
    }
}

Item Simulator::take_from(const Endpoint& from, Want want) {
    switch (from.kind) {
    case EndKind::InputChest:
        if (!accepts(Item::Plate, want)) return Item::None;
        ++counters_.plates_taken_from_input;
        return Item::Plate;
    case EndKind::Assembler: {
        Assembler& a = assemblers_[from.index];
        if (a.gears == 0 || !accepts(Item::Gear, want)) return Item::None;
        --a.gears;
        return Item::Gear;
    }
    default: return Item::None;  // belts: Task 4
    }
}

bool Simulator::put_into(const Inserter& ins, Item item) {
    switch (ins.to.kind) {
    case EndKind::Assembler:
        assert(item == Item::Plate);
        ++assemblers_[ins.to.index].plates;
        ++counters_.plates_into_assemblers;
        return true;
    case EndKind::OutputChest:
        if (item == Item::Gear) ++counters_.gears_delivered;
        else ++counters_.plates_into_output;
        return true;
    default: return false;  // belts: Task 4
    }
}

bool Simulator::conservation_holds() const {
    long long plates = counters_.plates_into_output;
    long long gears = counters_.gears_delivered;
    long long crafting = 0;
    auto count = [&](Item it) {
        if (it == Item::Plate) ++plates;
        else if (it == Item::Gear) ++gears;
    };
    for (const Assembler& a : assemblers_) {
        plates += a.plates;
        gears += a.gears;
        crafting += a.remaining > 0 ? 1 : 0;
    }
    for (const Inserter& ins : inserters_) count(ins.hand);
    for (const Belt& b : belts_)
        for (const auto& lane : b.slots)
            for (Item it : lane) count(it);
    return plates + cfg_.plates_per_gear * (counters_.gears_crafted + crafting) == counters_.plates_taken_from_input &&
           gears == counters_.gears_crafted;
}

SimResult Simulator::run() {
    run_ticks(cfg_.warmup_ticks);
    const SimCounters start = counters_;
    run_ticks(cfg_.measure_ticks);
    const double secs = cfg_.measure_ticks / 60.0;
    SimResult r;
    r.gears_delivered = (counters_.gears_delivered - start.gears_delivered) / secs;
    r.gears_crafted = (counters_.gears_crafted - start.gears_crafted) / secs;
    r.plates_into_assemblers = (counters_.plates_into_assemblers - start.plates_into_assemblers) / secs;
    r.plates_out_of_input = (counters_.plates_taken_from_input - start.plates_taken_from_input) / secs;
    r.entity_count = int(entities_.size());
    return r;
}

int Simulator::local(int entity, EntityType type) const {
    if (entity < 0 || entity >= int(entities_.size()) || entities_[entity].type != type)
        throw std::out_of_range("entity index " + std::to_string(entity) + " is not a " + factorio_name(type));
    return local_index_[entity];
}

int Simulator::assembler_plates(int entity) const { return assemblers_[local(entity, EntityType::Assembler)].plates; }
int Simulator::assembler_gears(int entity) const { return assemblers_[local(entity, EntityType::Assembler)].gears; }
Item Simulator::inserter_hand(int entity) const { return inserters_[local(entity, EntityType::Inserter)].hand; }

Item Simulator::belt_slot(int entity, int lane, int slot) const {
    return belts_[local(entity, EntityType::Belt)].slots.at(lane).at(slot);
}

void Simulator::set_assembler_plates(int entity, int plates) {
    checks_enabled_ = false;
    assemblers_[local(entity, EntityType::Assembler)].plates = plates;
}

void Simulator::set_belt_slot(int entity, int lane, int slot, Item item) {
    checks_enabled_ = false;
    belts_[local(entity, EntityType::Belt)].slots.at(lane).at(slot) = item;
}

SimResult simulate(const Layout& layout, const Config& cfg) { return Simulator(layout, cfg).run(); }

}  // namespace factory
