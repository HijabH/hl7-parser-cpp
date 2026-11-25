#include "hl7/message.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace hl7 {
namespace {

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        if (end == std::string_view::npos) {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\r' || text[i] == '\n') {
            if (i > start) lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

Repetitions parse_field(std::string_view raw, const Encoding& enc) {
    Repetitions repetitions;
    for (std::string_view rep : split(raw, enc.repetition)) {
        Components components;
        for (std::string_view comp : split(rep, enc.component)) {
            Subcomponents subs;
            for (std::string_view sub : split(comp, enc.subcomponent)) {
                subs.push_back(unescape(sub, enc));
            }
            components.push_back(std::move(subs));
        }
        repetitions.push_back(std::move(components));
    }
    return repetitions;
}

// A field holding one literal string that must not be split (MSH-1 and MSH-2).
Repetitions literal(std::string value) { return Repetitions{Components{Subcomponents{std::move(value)}}}; }

bool parse_int(std::string_view text, int& out) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc() && result.ptr == text.data() + text.size() && out >= 1;
}

}  // namespace

std::string unescape(std::string_view text, const Encoding& enc) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != enc.escape) {
            out += text[i];
            continue;
        }
        const std::size_t end = text.find(enc.escape, i + 1);
        if (end == std::string_view::npos) {  // lone escape character: keep it
            out += text[i];
            continue;
        }
        const std::string_view code = text.substr(i + 1, end - i - 1);
        if (code == "F") out += enc.field;
        else if (code == "S") out += enc.component;
        else if (code == "T") out += enc.subcomponent;
        else if (code == "R") out += enc.repetition;
        else if (code == "E") out += enc.escape;
        else out.append(text.substr(i, end - i + 1));  // unknown sequence: keep as is
        i = end;
    }
    return out;
}

std::string escape(std::string_view text, const Encoding& enc) {
    std::string out;
    for (char c : text) {
        if (c == enc.escape) out += std::string{enc.escape} + "E" + enc.escape;
        else if (c == enc.field) out += std::string{enc.escape} + "F" + enc.escape;
        else if (c == enc.component) out += std::string{enc.escape} + "S" + enc.escape;
        else if (c == enc.subcomponent) out += std::string{enc.escape} + "T" + enc.escape;
        else if (c == enc.repetition) out += std::string{enc.escape} + "R" + enc.escape;
        else out += c;
    }
    return out;
}

Message Message::parse(std::string_view text) {
    const auto lines = split_lines(text);
    if (lines.empty()) throw ParseError("empty message");

    const std::string_view header = lines.front();
    if (header.size() < 8 || header.substr(0, 3) != "MSH") {
        throw ParseError("message must start with an MSH segment");
    }

    Message message;
    Encoding& enc = message.encoding_;
    enc.field = header[3];
    const std::string_view encoding_chars = split(header.substr(4), enc.field).front();
    if (encoding_chars.size() < 4) {
        throw ParseError("MSH-2 must contain the four encoding characters, e.g. ^~\\&");
    }
    enc.component = encoding_chars[0];
    enc.repetition = encoding_chars[1];
    enc.escape = encoding_chars[2];
    enc.subcomponent = encoding_chars[3];

    for (std::size_t line_no = 0; line_no < lines.size(); ++line_no) {
        const std::string_view line = lines[line_no];
        const auto parts = split(line, enc.field);

        Segment segment;
        segment.name = std::string(parts.front());
        const bool name_ok = segment.name.size() == 3 &&
            std::all_of(segment.name.begin(), segment.name.end(),
                        [](unsigned char c) { return std::isupper(c) || std::isdigit(c); });
        if (!name_ok) {
            throw ParseError("segment " + std::to_string(line_no + 1) + " has an invalid name '" +
                             segment.name + "'");
        }

        segment.fields.emplace_back();  // fields[0] is unused
        if (segment.name == "MSH") {
            // MSH-1 is the field separator itself and MSH-2 the encoding characters
            segment.fields.push_back(literal(std::string(1, enc.field)));
            segment.fields.push_back(literal(std::string(parts[1])));
            for (std::size_t i = 2; i < parts.size(); ++i) segment.fields.push_back(parse_field(parts[i], enc));
        } else {
            for (std::size_t i = 1; i < parts.size(); ++i) segment.fields.push_back(parse_field(parts[i], enc));
        }
        message.segments_.push_back(std::move(segment));
    }
    return message;
}

const Segment* Message::segment(std::string_view name, int occurrence) const {
    int seen = 0;
    for (const Segment& s : segments_) {
        if (s.name == name && ++seen == occurrence) return &s;
    }
    return nullptr;
}

int Message::count(std::string_view name) const {
    return static_cast<int>(std::count_if(segments_.begin(), segments_.end(),
                                          [&](const Segment& s) { return s.name == name; }));
}

// Path grammar:  SEG[(occurrence)]-field[[repetition]][.component[.subcomponent]]
std::optional<std::string> Message::get(std::string_view path) const {
    const std::size_t dash = path.find('-');
    if (dash == std::string_view::npos || dash < 3) return std::nullopt;

    std::string_view seg_part = path.substr(0, dash);
    int occurrence = 1;
    if (const auto open = seg_part.find('('); open != std::string_view::npos) {
        if (seg_part.back() != ')' || !parse_int(seg_part.substr(open + 1, seg_part.size() - open - 2), occurrence)) {
            return std::nullopt;
        }
        seg_part = seg_part.substr(0, open);
    }

    std::string_view rest = path.substr(dash + 1);
    const auto dot_parts = split(rest, '.');
    if (dot_parts.size() > 3) return std::nullopt;

    std::string_view field_part = dot_parts[0];
    int repetition = 1;
    if (const auto open = field_part.find('['); open != std::string_view::npos) {
        if (field_part.back() != ']' ||
            !parse_int(field_part.substr(open + 1, field_part.size() - open - 2), repetition)) {
            return std::nullopt;
        }
        field_part = field_part.substr(0, open);
    }

    int field = 0, component = 1, subcomponent = 1;
    if (!parse_int(field_part, field)) return std::nullopt;
    if (dot_parts.size() > 1 && !parse_int(dot_parts[1], component)) return std::nullopt;
    if (dot_parts.size() > 2 && !parse_int(dot_parts[2], subcomponent)) return std::nullopt;

    const Segment* seg = segment(seg_part, occurrence);
    if (!seg || field >= static_cast<int>(seg->fields.size())) return std::nullopt;

    const Repetitions& reps = seg->fields[field];
    if (repetition > static_cast<int>(reps.size())) return std::nullopt;
    const Components& comps = reps[repetition - 1];

    // without a component number ("PID-5") the first component is returned
    if (component > static_cast<int>(comps.size())) return std::nullopt;
    const Subcomponents& subs = comps[component - 1];
    if (subcomponent > static_cast<int>(subs.size())) return std::nullopt;
    return subs[subcomponent - 1];
}

std::string Message::type() const {
    const std::string code = value("MSH-9.1");
    const std::string event = value("MSH-9.2");
    return event.empty() ? code : code + "^" + event;
}

std::string Message::control_id() const { return value("MSH-10"); }

std::vector<std::string> validate(const Message& message) {
    std::vector<std::string> issues;
    if (message.value("MSH-9.1").empty()) issues.emplace_back("MSH-9 message type is missing");
    if (message.control_id().empty()) issues.emplace_back("MSH-10 message control ID is missing");
    if (message.value("MSH-12").empty()) issues.emplace_back("MSH-12 HL7 version is missing");

    const std::string code = message.value("MSH-9.1");
    if (code == "ADT" || code == "ORU" || code == "ORM") {
        if (!message.segment("PID")) {
            issues.emplace_back(code + " message has no PID segment");
        } else if (message.value("PID-3.1").empty()) {
            issues.emplace_back("PID-3 patient identifier is missing");
        }
    }
    if (code == "ADT" && !message.segment("EVN")) issues.emplace_back("ADT message has no EVN segment");
    if (code == "ORU" && message.count("OBX") == 0) issues.emplace_back("ORU message has no OBX result segment");
    return issues;
}

std::optional<Patient> patient(const Message& message) {
    if (!message.segment("PID")) return std::nullopt;
    return Patient{message.value("PID-3.1"), message.value("PID-5.1"), message.value("PID-5.2"),
                   message.value("PID-7"), message.value("PID-8")};
}

std::string make_ack(const Message& original, AckCode code, std::string_view text,
                     std::string_view timestamp, std::string_view control_id) {
    const Encoding& e = original.encoding();
    const char f = e.field;
    const std::string enc_chars{e.component, e.repetition, e.escape, e.subcomponent};
    const char* ack_code = code == AckCode::Accept ? "AA" : code == AckCode::Error ? "AE" : "AR";

    // the ACK goes back to the sender, so sending and receiving parties are swapped
    std::string out = "MSH";
    out += f + enc_chars + f + escape(original.value("MSH-5"), e) + f + escape(original.value("MSH-6"), e) + f +
           escape(original.value("MSH-3"), e) + f + escape(original.value("MSH-4"), e) + f + std::string(timestamp) +
           f + f + "ACK" + e.component + escape(original.value("MSH-9.2"), e) + f + std::string(control_id) + f +
           escape(original.value("MSH-11"), e) + f + escape(original.value("MSH-12"), e) + '\r';
    out += "MSA";
    out += f + std::string(ack_code) + f + escape(original.control_id(), e);
    if (!text.empty()) out += f + escape(text, e);
    out += '\r';
    return out;
}

}  // namespace hl7
