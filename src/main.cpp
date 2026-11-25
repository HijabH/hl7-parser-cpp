// hl7tool: command-line front end for the parser.
//
//   hl7tool json     <file>          print the message as JSON
//   hl7tool get      <file> <path>   print one value, e.g. PID-5.1
//   hl7tool validate <file>          check the structure, exit code 1 if invalid
//   hl7tool ack      <file>          print the ACK a receiving system would send
//   hl7tool patient  <file>          print the patient from PID
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

#include "hl7/message.hpp"

namespace {

int usage() {
    std::cerr << "Usage: hl7tool <json|get|validate|ack|patient> <file> [path]\n";
    return 2;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string now_timestamp() {
    const std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y%m%d%H%M%S", std::gmtime(&t));
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) return usage();
    const std::string command = argv[1];

    try {
        const hl7::Message message = hl7::Message::parse(read_file(argv[2]));

        if (command == "json") {
            std::cout << message.to_json() << '\n';
        } else if (command == "get") {
            if (argc < 4) return usage();
            const auto value = message.get(argv[3]);
            if (!value) {
                std::cerr << "not found: " << argv[3] << '\n';
                return 1;
            }
            std::cout << *value << '\n';
        } else if (command == "validate") {
            const auto issues = hl7::validate(message);
            if (issues.empty()) {
                std::cout << "valid " << message.type() << " message (" << message.segments().size()
                          << " segments)\n";
                return 0;
            }
            for (const auto& issue : issues) std::cout << "invalid: " << issue << '\n';
            return 1;
        } else if (command == "ack") {
            const auto issues = hl7::validate(message);
            const auto code = issues.empty() ? hl7::AckCode::Accept : hl7::AckCode::Error;
            std::string ack = hl7::make_ack(message, code, issues.empty() ? "" : issues.front(), now_timestamp(),
                                            "ACK" + message.control_id());
            for (char& c : ack)
                if (c == '\r') c = '\n';  // readable on a terminal
            std::cout << ack;
        } else if (command == "patient") {
            const auto p = hl7::patient(message);
            if (!p) {
                std::cerr << "no PID segment\n";
                return 1;
            }
            std::cout << "id: " << p->id << "\nname: " << p->given_name << ' ' << p->family_name
                      << "\nbirth date: " << p->birth_date << "\nsex: " << p->sex << '\n';
        } else {
            return usage();
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
