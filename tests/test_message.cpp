#include <string>

#include "hl7/message.hpp"
#include "minitest.hpp"

using hl7::Message;

namespace {

const std::string ADT =
    "MSH|^~\\&|ADMISSIONS|CITY_HOSPITAL|LAB|CITY_HOSPITAL|20261124093000||ADT^A01^ADT_A01|MSG00001|P|2.5\r"
    "EVN|A01|20261124093000\r"
    "PID|1||100234^^^CITY_HOSPITAL^MR~555-44-3333^^^INSURER^SS||Mustermann^Erika^M||19820412|F\r"
    "PV1|1|I|WARD3^305^B^CITY_HOSPITAL\r";

const std::string ORU =
    "MSH|^~\\&|LAB_SYSTEM|CITY_HOSPITAL|EHR|CITY_HOSPITAL|20261124120000||ORU^R01|LAB778899|P|2.5\r"
    "PID|1||100234^^^CITY_HOSPITAL^MR||Mustermann^Erika\r"
    "OBX|1|NM|718-7^Hemoglobin^LN||13.2|g/dL\r"
    "OBX|2|NM|6690-2^Leukocytes^LN||11.8|10*3/uL\r";

}  // namespace

// --- parsing -------------------------------------------------------------------

TEST(parses_segments_in_order) {
    const Message m = Message::parse(ADT);
    CHECK_EQ(m.segments().size(), std::size_t{4});
    CHECK_EQ(m.segments()[0].name, std::string("MSH"));
    CHECK_EQ(m.segments()[3].name, std::string("PV1"));
}

TEST(msh_fields_follow_the_standard_numbering) {
    const Message m = Message::parse(ADT);
    CHECK_EQ(m.value("MSH-1"), std::string("|"));
    CHECK_EQ(m.value("MSH-2"), std::string("^~\\&"));
    CHECK_EQ(m.value("MSH-3"), std::string("ADMISSIONS"));
    CHECK_EQ(m.value("MSH-10"), std::string("MSG00001"));
    CHECK_EQ(m.value("MSH-12"), std::string("2.5"));
}

TEST(reads_message_type_and_control_id) {
    const Message m = Message::parse(ADT);
    CHECK_EQ(m.type(), std::string("ADT^A01"));
    CHECK_EQ(m.control_id(), std::string("MSG00001"));
}

TEST(reads_components_and_repetitions) {
    const Message m = Message::parse(ADT);
    CHECK_EQ(m.value("PID-5.1"), std::string("Mustermann"));
    CHECK_EQ(m.value("PID-5.2"), std::string("Erika"));
    CHECK_EQ(m.value("PID-5"), std::string("Mustermann"));  // no component = first component
    CHECK_EQ(m.value("PID-3.1"), std::string("100234"));
    CHECK_EQ(m.value("PID-3[2].1"), std::string("555-44-3333"));
    CHECK_EQ(m.value("PID-3[2].5"), std::string("SS"));
}

TEST(reads_repeated_segments_by_occurrence) {
    const Message m = Message::parse(ORU);
    CHECK_EQ(m.count("OBX"), 2);
    CHECK_EQ(m.value("OBX-5"), std::string("13.2"));
    CHECK_EQ(m.value("OBX(2)-5"), std::string("11.8"));
    CHECK_EQ(m.value("OBX(2)-3.2"), std::string("Leukocytes"));
}

TEST(reads_subcomponents) {
    const Message m = Message::parse("MSH|^~\\&|A|B|C|D|20260101||ADT^A01|1|P|2.5\rPID|1||id^^^HOSP&1.2.3&ISO\r");
    CHECK_EQ(m.value("PID-3.4"), std::string("HOSP"));
    CHECK_EQ(m.value("PID-3.4.2"), std::string("1.2.3"));
    CHECK_EQ(m.value("PID-3.4.3"), std::string("ISO"));
}

TEST(missing_values_return_nullopt) {
    const Message m = Message::parse(ADT);
    CHECK(!m.get("PID-99").has_value());
    CHECK(!m.get("PID-5.9").has_value());
    CHECK(!m.get("PID-3[5].1").has_value());
    CHECK(!m.get("ZZZ-1").has_value());
    CHECK(!m.get("OBX(2)-5").has_value());
}

TEST(rejects_malformed_paths) {
    const Message m = Message::parse(ADT);
    CHECK(!m.get("PID").has_value());
    CHECK(!m.get("PID-x").has_value());
    CHECK(!m.get("PID-0").has_value());
    CHECK(!m.get("PID-5.1.1.1").has_value());
    CHECK(!m.get("PID-3[0]").has_value());
    CHECK(!m.get("OBX(a)-5").has_value());
}

TEST(accepts_newline_and_crlf_segment_endings) {
    std::string lf = ADT, crlf;
    for (char& c : lf)
        if (c == '\r') c = '\n';
    for (char c : ADT) crlf += c == '\r' ? std::string("\r\n") : std::string(1, c);
    CHECK_EQ(Message::parse(lf).value("PID-5.2"), std::string("Erika"));
    CHECK_EQ(Message::parse(crlf).segments().size(), std::size_t{4});
}

TEST(uses_the_delimiters_declared_in_msh) {
    const Message m = Message::parse("MSH#*~\\&#APP#FAC#X#Y#20260101##ADT*A01#7#P#2.5\rPID#1##123*4*5##Doe*John\r");
    CHECK_EQ(m.encoding().field, '#');
    CHECK_EQ(m.type(), std::string("ADT^A01"));
    CHECK_EQ(m.value("PID-5.2"), std::string("John"));
}

TEST(rejects_messages_without_msh) {
    CHECK_THROWS(Message::parse(""), hl7::ParseError);
    CHECK_THROWS(Message::parse("PID|1||123\r"), hl7::ParseError);
    CHECK_THROWS(Message::parse("MSH|^~\r"), hl7::ParseError);
}

TEST(rejects_invalid_segment_names) {
    CHECK_THROWS(Message::parse("MSH|^~\\&|A\rpid|1\r"), hl7::ParseError);
    CHECK_THROWS(Message::parse("MSH|^~\\&|A\rPIDX|1\r"), hl7::ParseError);
}

// --- escaping ------------------------------------------------------------------

TEST(unescapes_standard_sequences) {
    const hl7::Encoding e;
    CHECK_EQ(hl7::unescape("A \\T\\ B", e), std::string("A & B"));
    CHECK_EQ(hl7::unescape("x\\F\\y\\S\\z\\R\\w\\E\\", e), std::string("x|y^z~w\\"));
    CHECK_EQ(hl7::unescape("keep \\H\\ unknown", e), std::string("keep \\H\\ unknown"));
    CHECK_EQ(hl7::unescape("lone \\ escape", e), std::string("lone \\ escape"));
}

TEST(escape_and_unescape_round_trip) {
    const hl7::Encoding e;
    const std::string text = "a|b^c&d~e\\f";
    CHECK_EQ(hl7::unescape(hl7::escape(text, e), e), text);
}

TEST(escaped_delimiters_do_not_split_fields) {
    const Message m = Message::parse("MSH|^~\\&|A|B|C|D|1||ORU^R01|1|P|2.5\rOBX|1|ST|N||Smith \\T\\ Sons\\F\\ltd\r");
    CHECK_EQ(m.value("OBX-5"), std::string("Smith & Sons|ltd"));
}

// --- validation, patient, ACK ----------------------------------------------------

TEST(valid_messages_have_no_issues) {
    CHECK(hl7::validate(Message::parse(ADT)).empty());
    CHECK(hl7::validate(Message::parse(ORU)).empty());
}

TEST(validation_finds_missing_segments_and_fields) {
    const auto issues = hl7::validate(Message::parse("MSH|^~\\&|A|B|C|D|1||ADT^A01||P\rPID|1\r"));
    auto has = [&](const std::string& text) {
        for (const auto& i : issues)
            if (i.find(text) != std::string::npos) return true;
        return false;
    };
    CHECK(has("MSH-10"));
    CHECK(has("MSH-12"));
    CHECK(has("PID-3"));
    CHECK(has("no EVN"));
}

TEST(extracts_the_patient) {
    const auto p = hl7::patient(Message::parse(ADT));
    CHECK(p.has_value());
    CHECK_EQ(p->id, std::string("100234"));
    CHECK_EQ(p->family_name, std::string("Mustermann"));
    CHECK_EQ(p->given_name, std::string("Erika"));
    CHECK_EQ(p->birth_date, std::string("19820412"));
    CHECK_EQ(p->sex, std::string("F"));
}

TEST(builds_an_ack_that_parses_back) {
    const Message original = Message::parse(ADT);
    const std::string ack = hl7::make_ack(original, hl7::AckCode::Accept, "", "20261124093005", "ACK0001");
    const Message reply = Message::parse(ack);

    CHECK_EQ(reply.type(), std::string("ACK^A01"));
    CHECK_EQ(reply.value("MSH-3"), std::string("LAB"));         // sender and receiver swapped
    CHECK_EQ(reply.value("MSH-5"), std::string("ADMISSIONS"));
    CHECK_EQ(reply.value("MSA-1"), std::string("AA"));
    CHECK_EQ(reply.value("MSA-2"), std::string("MSG00001"));  // refers to the original message
}

TEST(error_ack_carries_escaped_text) {
    const Message original = Message::parse(ADT);
    const std::string ack = hl7::make_ack(original, hl7::AckCode::Error, "PID-3 missing | check", "1", "2");
    const Message reply = Message::parse(ack);
    CHECK_EQ(reply.value("MSA-1"), std::string("AE"));
    CHECK_EQ(reply.value("MSA-3"), std::string("PID-3 missing | check"));
}

TEST(json_contains_nested_values) {
    const std::string json = Message::parse(ADT).to_json();
    CHECK(json.find("\"type\":\"ADT^A01\"") != std::string::npos);
    CHECK(json.find("[\"Mustermann\",\"Erika\",\"M\"]") != std::string::npos);
    CHECK(json.find("\"MSH-1\"") == std::string::npos);
}

int main() { return minitest::run_all(); }
