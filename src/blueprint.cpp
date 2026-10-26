#include "blueprint.hpp"

#include <zlib.h>

#include <stdexcept>

namespace factory {
namespace {

constexpr unsigned long long kVersion_2_0_10 = (2ULL << 48) | (0ULL << 32) | (10ULL << 16);
constexpr char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Factorio 2.0 uses 16 directions: north 0, east 4, south 8, west 12.
int factorio_direction(Direction d) { return int(d) * 4; }

nlohmann::json entity(int number, const char* name, double x, double y) {
    return {{"entity_number", number}, {"name", name}, {"position", {{"x", x}, {"y", y}}}};
}

void set_direction(nlohmann::json& e, Direction d) {
    if (const int fd = factorio_direction(d); fd != 0) e["direction"] = fd;  // north is the default
}

std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(kBase64[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(kBase64[((val << 8) >> (bits + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in) {
    std::string out;
    int val = 0, bits = -8;
    for (char c : in) {
        if (c == '=') break;
        const char* p = std::char_traits<char>::find(kBase64, 64, c);
        if (!p) throw std::runtime_error("blueprint string is not valid base64");
        val = (val << 6) + int(p - kBase64);
        bits += 6;
        if (bits >= 0) {
            out.push_back(char((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

std::string zlib_compress(const std::string& in) {
    uLongf size = compressBound(uLong(in.size()));
    std::string out(size, '\0');
    if (compress2(reinterpret_cast<Bytef*>(out.data()), &size, reinterpret_cast<const Bytef*>(in.data()),
                  uLong(in.size()), 9) != Z_OK)
        throw std::runtime_error("zlib compression failed");
    out.resize(size);
    return out;
}

std::string zlib_decompress(const std::string& in) {
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) throw std::runtime_error("zlib init failed");
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    zs.avail_in = uInt(in.size());
    std::string out;
    char buf[16384];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            throw std::runtime_error("blueprint string is not valid zlib data");
        }
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (ret != Z_STREAM_END && (zs.avail_in > 0 || zs.avail_out == 0));
    inflateEnd(&zs);
    if (ret != Z_STREAM_END) throw std::runtime_error("blueprint string is not valid zlib data (truncated)");
    return out;
}

}  // namespace

nlohmann::json blueprint_json(const Layout& l) {
    using nlohmann::json;
    json entities = json::array();
    auto add = [&](const char* name, double x, double y) -> json& {
        entities.push_back(entity(int(entities.size()) + 1, name, x, y));
        return entities.back();
    };

    json& in = add("infinity-chest", l.input_chest().x + 0.5, l.input_chest().y + 0.5);
    in["infinity_settings"] = {{"remove_unfiltered_items", false},
                               {"filters", json::array({{{"index", 1}, {"name", "iron-plate"}, {"count", 100},
                                                         {"mode", "at-least"}}})}};
    add("wooden-chest", l.output_chest().x + 0.5, l.output_chest().y + 0.5);

    for (const Entity& e : l.entities()) {
        const double half = entity_size(e.type) / 2.0;
        json& b = add(factorio_name(e.type), e.x + half, e.y + half);
        switch (e.type) {
        case EntityType::Assembler: b["recipe"] = "iron-gear-wheel"; break;
        case EntityType::Belt: set_direction(b, e.dir); break;
        case EntityType::Inserter: set_direction(b, opposite(e.dir)); break;  // Factorio stores the pickup side
        }
    }

    // A self-powered substation (18x18 supply area) on each side of the grid; no wires needed.
    const double mid = l.size() / 2;
    for (const double cx : {-1.0, l.size() + 1.0}) {
        add("substation", cx, mid);
        add("electric-energy-interface", cx, mid + 2);
    }

    return {{"blueprint",
             {{"item", "blueprint"},
              {"label", "factory: iron gears"},
              {"icons", json::array({{{"index", 1}, {"signal", {{"type", "item"}, {"name", "iron-gear-wheel"}}}}})},
              {"entities", entities},
              {"version", kVersion_2_0_10}}}};
}

std::string blueprint_string(const Layout& layout) {
    return "0" + base64_encode(zlib_compress(blueprint_json(layout).dump()));
}

nlohmann::json decode_blueprint_string(const std::string& s) {
    if (s.empty()) throw std::runtime_error("blueprint string is empty");
    if (s[0] != '0') throw std::runtime_error("unsupported blueprint string version byte '" + s.substr(0, 1) + "'");
    const std::string text = zlib_decompress(base64_decode(s.substr(1)));
    try {
        return nlohmann::json::parse(text);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("blueprint string does not contain JSON: ") + e.what());
    }
}

}  // namespace factory
