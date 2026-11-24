if(NOT DEFINED CLI OR NOT DEFINED CJC OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "CLI, CJC, and TEST_DIR are required")
endif()
file(MAKE_DIRECTORY "${TEST_DIR}")

function(run_case name source expected)
    file(WRITE "${TEST_DIR}/${name}.typed" "${source}")
    execute_process(COMMAND "${CLI}" INPUT_FILE "${TEST_DIR}/${name}.typed"
        RESULT_VARIABLE status OUTPUT_VARIABLE generated ERROR_VARIABLE error)
    if(NOT status EQUAL 0 OR NOT error STREQUAL "")
        message(FATAL_ERROR "${name}: translation failed: ${error}")
    endif()
    # Test the full Int64 result instead of the operating system's truncated exit
    # status. The actual cj command preserves main's return value without printing.
    string(REPLACE "main(): Int64 {" "func typedTestMain(): Int64 {" tested "${generated}")
    string(APPEND tested "\nmain(): Int64 {\n    println(typedTestMain())\n    return 0\n}\n")
    file(WRITE "${TEST_DIR}/${name}.cj" "${tested}")
    execute_process(COMMAND "${CJC}" "${TEST_DIR}/${name}.cj" -o "${TEST_DIR}/${name}"
        --set-runtime-rpath --diagnostic-format noColor
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${name}: generated Cangjie did not compile: ${output}${error}")
    endif()
    execute_process(COMMAND "${TEST_DIR}/${name}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
    if(expected STREQUAL "THROWS")
        if(status STREQUAL "0" OR NOT "${output}${error}" MATCHES "exited without returning")
            message(FATAL_ERROR "${name}: expected fallthrough exception, got ${status}: ${output}${error}")
        endif()
    elseif(NOT status EQUAL 0 OR NOT output STREQUAL "${expected}\n")
        message(FATAL_ERROR "${name}: expected ${expected}, got ${status}: ${output}${error}")
    endif()
endfunction()

foreach(example fibonacci-5 fibonacci-5-break functions)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/../examples/${example}.typed" source)
    if(example STREQUAL "functions")
        set(expected 120)
    else()
        set(expected 5)
    endif()
    run_case("${example}" "${source}" "${expected}")
endforeach()

run_case(collections "main(): Int64 {
    let empty: ListInt64 = emptyList();
    let first: ListInt64 = listPush(empty, 4);
    var items: ListInt64 = first;
    let copy: ListInt64 = items;
    let second: ListInt64 = listPush(items, 5);
    items = second;
    let n: Int64 = listLen(copy);
    let text: String = \"a b\";
    let letter: String = text[0];
    let joined: String = text + letter;
    let same: Bool = letter == \"a\";
    let length: Int64 = strLen(joined);
    let total: Int64 = n + length;
    if(same){return total;}else{return 0;}
}" 5)

run_case(builtins "main(): Int64 {
    let sum: Int64 = 2 + 3; let difference: Int64 = sum - 1;
    let product: Int64 = difference * 3; let small: Bool = product < 13;
    let same: Bool = product == 12; let both: Bool = small && same;
    let inverse: Bool = !false; let either: Bool = both || inverse;
    if(either){return product;}else{return 0;}
}" 12)

run_case(copies "main(): Int64 {
    var original: Int64 = 7; let copy: Int64 = original;
    let next: Int64 = original + 1; original = next; return copy;
}" 7)

run_case(bool_vars "main(): Int64 {
    var flag: Bool = true; var count: Int64 = 0;
    while(flag){
        let next: Int64 = count + 1; count = next;
        let done: Bool = next == 3; if(done){flag = false;}else{}
    }
    return count;
}" 3)

run_case(loop_control "main(): Int64 {
    var i: Int64 = 0; var total: Int64 = 0;
    while(true){
        let done: Bool = i == 3; if(done){break;}else{}
        let next: Int64 = i + 1; i = next;
        while(true){let incremented: Int64 = total + 2; total = incremented; break;}
        let skip: Bool = next == 2; if(skip){continue;}else{}
        let incremented: Int64 = total + 1; total = incremented;
    }
    return total;
}" 8)

run_case(loop_return "main(): Int64 {
    while(true){if(true){return 7;}else{break;}} return 8;
}" 7)

run_case(target_keywords "func class(this: Int64): Int64 {
    let private: Int64 = this + 1; return private;
}
main(): Int64 { let match: Int64 = class(4); return match; }" 5)

run_case(later_names "func first(): Int64 { let second: Int64 = 3; return second; }
func second(): Int64 { return 4; }
main(): Int64 { let a: Int64 = first(); let b: Int64 = second();
    let sum: Int64 = a + b; return sum; }" 7)

run_case(sibling_scopes "main(): Int64 {
    if(true){let same: Int64 = -001; return same;}else{let same: Int64 = 2; return same;}
}" -1)
run_case(minimum "main(): Int64 { return -0009223372036854775808; }" -9223372036854775808)
run_case(maximum "main(): Int64 { var r: Int64 = 9223372036854775807; return r; }" 9223372036854775807)
run_case(zero "main(): Int64 { return -0000000000; }" 0)
run_case(unreachable "main(): Int64 { return 1; let unused: Int64 = 2; return unused; }" 1)
run_case(empty "main(): Int64 {}" THROWS)
run_case(fallthrough "func maybe(flag: Bool): Int64 {
    if(flag){return 1;}else{}
}
main(): Int64 { let r: Int64 = maybe(false); return r; }" THROWS)

set(parameters "")
set(arguments "")
foreach(index RANGE 0 39)
    if(index GREATER 0)
        string(APPEND parameters ",")
        string(APPEND arguments ",")
    endif()
    string(APPEND parameters "p${index}: Int64")
    string(APPEND arguments "${index}")
endforeach()
run_case(long_signature "func last(${parameters}): Int64 { return p39; }
main(): Int64 { let result: Int64 = last(${arguments}); return result; }" 39)

message(STATUS "Generated Cangjie compiled and ran all semantic fixtures")
