// JSON output for a parsed message. Simple fields become strings; fields with
// components, subcomponents or repetitions become nested arrays. Empty fields are left out.
#include "hl7/message.hpp"

#include <algorithm>
#include <cstdio>

namespace hl7 {
namespace {

void append_string(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

bool empty(const Components& comps) {
    for (const auto& subs : comps)
        for (const auto& s : subs)
            if (!s.empty()) return false;
    return true;
}

void append_subs(std::string& out, const Subcomponents& subs) {
    if (subs.size() == 1) return append_string(out, subs[0]);
    out += '[';
    for (std::size_t i = 0; i < subs.size(); ++i) {
        if (i) out += ',';
        append_string(out, subs[i]);
    }
    out += ']';
}

void append_components(std::string& out, const Components& comps) {
    if (comps.size() == 1) return append_subs(out, comps[0]);
    out += '[';
    for (std::size_t i = 0; i < comps.size(); ++i) {
        if (i) out += ',';
        append_subs(out, comps[i]);
    }
    out += ']';
}

}  // namespace

std::string Message::to_json() const {
    std::string out = "{\"type\":";
    append_string(out, type());
    out += ",\"controlId\":";
    append_string(out, control_id());
    out += ",\"segments\":[";

    for (std::size_t s = 0; s < segments_.size(); ++s) {
        const Segment& seg = segments_[s];
        if (s) out += ',';
        out += "{\"name\":";
        append_string(out, seg.name);
        out += ",\"fields\":{";
        bool first = true;
        for (std::size_t f = 1; f < seg.fields.size(); ++f) {
            const Repetitions& reps = seg.fields[f];
            const bool all_empty = std::all_of(reps.begin(), reps.end(), [](const Components& c) { return empty(c); });
            if (all_empty) continue;
            if (!first) out += ',';
            first = false;
            out += '"' + std::to_string(f) + "\":";
            if (reps.size() == 1) {
                append_components(out, reps[0]);
            } else {
                out += '[';
                for (std::size_t r = 0; r < reps.size(); ++r) {
                    if (r) out += ',';
                    append_components(out, reps[r]);
                }
                out += ']';
            }
        }
        out += "}}";
    }
    out += "]}";
    return out;
}

}  // namespace hl7
