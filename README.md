# HL7 v2 Parser (C++)

![build-and-test](https://github.com/HijabH/hl7-parser-cpp/actions/workflows/ci.yml/badge.svg)

A small C++17 library and command-line tool for reading **HL7 version 2** messages, the pipe-delimited format hospitals use to exchange admissions, discharges, lab results and orders between their systems.

I worked for years on national health information systems, where getting patient data from one system into another correctly is most of the job. This project is my way of learning modern C++ on a problem from that domain.

## Features

- Parses segments, fields, repetitions, components and subcomponents
- Uses the delimiters each message declares in `MSH-1` and `MSH-2`
- Handles HL7 escape sequences (`\F\ \S\ \T\ \R\ \E\`) in both directions
- Looks up values by path: `PID-5.1`, `PID-3[2].1`, `OBX(2)-5`, `PID-3.4.2`
- Validates the structure a receiving system would check (message type, control ID, version, PID, EVN, OBX)
- Extracts the patient from `PID`
- Builds the acknowledgement (`ACK` with `AA` or `AE`) a receiving system sends back
- Converts a message to JSON
- Accepts `\r` (the standard), `\n` or `\r\n` between segments
- No external dependencies; builds with warnings as errors and runs its tests under AddressSanitizer and UndefinedBehaviorSanitizer

## Build and test

Requires CMake 3.16+ and a C++17 compiler (GCC, Clang or MSVC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DHL7_SANITIZE=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows with Visual Studio, open the folder (CMake support is built in) or leave out `-DHL7_SANITIZE=ON`.

## Command line

```text
$ hl7tool validate samples/oru_r01_lab_result.hl7
valid ORU^R01 message (6 segments)

$ hl7tool get samples/oru_r01_lab_result.hl7 "OBX(2)-5"
11.8

$ hl7tool get samples/oru_r01_lab_result.hl7 "OBX(3)-5"
Sample slightly hemolysed & re-check advised

$ hl7tool patient samples/adt_a01_admit.hl7
id: 100234
name: Erika Mustermann
birth date: 19820412
sex: F

$ hl7tool validate samples/invalid_adt_missing_fields.hl7
invalid: PID-3 patient identifier is missing
invalid: ADT message has no EVN segment

$ hl7tool ack samples/invalid_adt_missing_fields.hl7
MSH|^~\&|LAB|CITY_HOSPITAL|ADMISSIONS|CITY_HOSPITAL|20261124...||ACK^A01|ACKMSG00003|P|2.5
MSA|AE|MSG00003|PID-3 patient identifier is missing

$ hl7tool json samples/adt_a03_discharge.hl7
{"type":"ADT^A03","controlId":"MSG00002","segments":[{"name":"MSH","fields":{"1":"|", ...
```

## Using the library

```cpp
#include "hl7/message.hpp"

const auto msg = hl7::Message::parse(text);

std::string family = msg.value("PID-5.1");        // "Mustermann"
auto insurance_id  = msg.get("PID-3[2].1");       // std::optional<std::string>
std::string result = msg.value("OBX(2)-5");       // second OBX segment, field 5

if (auto issues = hl7::validate(msg); !issues.empty()) {
    auto ack = hl7::make_ack(msg, hl7::AckCode::Error, issues.front(), timestamp, "ACK1");
}
```

## Design

- **Parse once, keep it simple.** A message becomes a vector of segments; each field is stored fully split (`repetitions > components > subcomponents`) and already unescaped, so lookups never parse again.
- **HL7 numbering everywhere.** `fields[n]` is field *n* as written in the standard, including the special `MSH-1` and `MSH-2`, which avoids off-by-one mistakes when reading the specification next to the code.
- **Missing is not an error.** `get()` returns `std::optional`, because optional fields are normal in HL7.
- **Strict where it matters.** Structural problems (no `MSH`, invalid segment names, missing encoding characters) throw `hl7::ParseError`; business rules are reported by `validate()`, so the caller decides whether to send `AE` or reject.

## Tests

21 unit tests cover parsing, paths, repetitions, subcomponents, custom delimiters, line endings, escaping, validation, patient extraction, ACK generation and JSON. Five more tests run `hl7tool` on the sample messages. GitHub Actions builds with GCC and Clang, with sanitizers, on every push.

## Limitations

- HL7 v2 only (not FHIR or v3), and no batch (`FHS`/`BHS`) or MLLP network framing
- Validation covers common structural rules, not every message profile in the standard
- Hex (`\X..\`) and formatting escape sequences are kept as they are, not decoded

## License

[MIT](LICENSE)
