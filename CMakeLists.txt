cmake_minimum_required(VERSION 3.16)
project(hl7_parser VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HL7_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
  if(HL7_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

add_library(hl7 src/message.cpp src/json.cpp)
target_include_directories(hl7 PUBLIC include)

add_executable(hl7tool src/main.cpp)
target_link_libraries(hl7tool PRIVATE hl7)

enable_testing()
add_executable(hl7_tests tests/test_message.cpp)
target_link_libraries(hl7_tests PRIVATE hl7)
add_test(NAME unit_tests COMMAND hl7_tests)

# end-to-end checks of the command-line tool on the sample messages
add_test(NAME cli_validate_admit COMMAND hl7tool validate ${CMAKE_SOURCE_DIR}/samples/adt_a01_admit.hl7)
add_test(NAME cli_validate_lab COMMAND hl7tool validate ${CMAKE_SOURCE_DIR}/samples/oru_r01_lab_result.hl7)
add_test(NAME cli_invalid_message_fails COMMAND hl7tool validate ${CMAKE_SOURCE_DIR}/samples/invalid_adt_missing_fields.hl7)
set_tests_properties(cli_invalid_message_fails PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_get_name COMMAND hl7tool get ${CMAKE_SOURCE_DIR}/samples/adt_a01_admit.hl7 PID-5.2)
set_tests_properties(cli_get_name PROPERTIES PASS_REGULAR_EXPRESSION "^Erika")
