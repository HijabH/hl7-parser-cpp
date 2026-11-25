// HL7 v2 message parser.
//
//   PID|1||12345^^^HOSP^MR~98765^^^INS^SS||Doe^Jane^Q||19800101|F
//          field 3, repetition 2, component 1 = "98765"
#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace hl7 {

// The delimiters are declared in MSH-1 and MSH-2, so every message carries its own.
struct Encoding {
    char field = '|';
    char component = '^';
    char repetition = '~';
    char escape = '\\';
    char subcomponent = '&';
};

using Subcomponents = std::vector<std::string>;  // already unescaped
using Components = std::vector<Subcomponents>;
using Repetitions = std::vector<Components>;

struct Segment {
    std::string name;
    // fields[n] is field n, numbered as in the HL7 standard; fields[0] is unused.
    std::vector<Repetitions> fields;
};

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& message) : std::runtime_error(message) {}
};

class Message {
public:
    // Parses a message. Segments may end with \r (the standard), \n or \r\n.
    static Message parse(std::string_view text);

    // Value at a path such as "PID-5.1", "PID-3[2].1", "OBX(2)-5" or "PID-5.1.1".
    // All numbers are 1-based. Returns std::nullopt if the path does not exist.
    std::optional<std::string> get(std::string_view path) const;

    std::string value(std::string_view path) const { return get(path).value_or(""); }

    const Segment* segment(std::string_view name, int occurrence = 1) const;
    int count(std::string_view name) const;

    std::string type() const;        // MSH-9, e.g. "ADT^A01"
    std::string control_id() const;  // MSH-10

    const std::vector<Segment>& segments() const { return segments_; }
    const Encoding& encoding() const { return encoding_; }

    std::string to_json() const;

private:
    std::vector<Segment> segments_;
    Encoding encoding_;
};

// Replaces HL7 escape sequences (\F\ \S\ \T\ \R\ \E\) with the characters they stand for.
std::string unescape(std::string_view text, const Encoding& enc);

std::string escape(std::string_view text, const Encoding& enc);

// Structural checks a receiving system would make before accepting a message.
std::vector<std::string> validate(const Message& message);

struct Patient {
    std::string id;          // PID-3.1, first repetition
    std::string family_name; // PID-5.1
    std::string given_name;  // PID-5.2
    std::string birth_date;  // PID-7, YYYYMMDD
    std::string sex;         // PID-8
};

std::optional<Patient> patient(const Message& message);

enum class AckCode { Accept, Error, Reject };  // AA, AE, AR

// timestamp is passed in (YYYYMMDDHHMMSS) so the output is predictable in tests.
std::string make_ack(const Message& original, AckCode code, std::string_view text,
                     std::string_view timestamp, std::string_view control_id);

}  // namespace hl7
