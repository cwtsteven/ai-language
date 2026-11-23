if(NOT DEFINED CLI OR NOT DEFINED CJC OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "CLI, CJC, and TEST_DIR are required")
endif()
file(MAKE_DIRECTORY "${TEST_DIR}")

function(run_case name source expected)
    file(WRITE "${TEST_DIR}/${name}.typed" "${source}")
    execute_process(COMMAND "${CLI}" cj - INPUT_FILE "${TEST_DIR}/${name}.typed"
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

run_case(builtins "main(): Int64 {
    let sum: Int64 = add(2,3); let difference: Int64 = sub(sum,1);
    let product: Int64 = mul(difference,3); let small: Bool = lt(product,13);
    let same: Bool = eq(product,12); let both: Bool = and(small,same);
    let inverse: Bool = not(false); let either: Bool = or(both,inverse);
    if(either){return product;}else{return 0;}
}" 12)

run_case(aliases "func identity(r: RefInt64): RefInt64 { return r; }
func bump(r: RefInt64): Int64 {
    let next: Int64 = add(!r,1); assign(r,next); return !r;
}
main(): Int64 {
    let original: RefInt64 = refInt(7); let alias: RefInt64 = identity(original);
    let updated: Int64 = bump(alias); return !original;
}" 8)

run_case(bool_cells "func identity(r: RefBool): RefBool { return r; }
func create(): RefBool { let flag: RefBool = refBool(true); return flag; }
main(): Int64 {
    let flag: RefBool = create(); let alias: RefBool = identity(flag);
    let count: RefInt64 = refInt(0);
    while(!flag){
        let next: Int64 = add(!count,1); assign(count,next);
        let done: Bool = eq(next,3); if(done){assign(alias,false);}else{}
    }
    return !count;
}" 3)

run_case(loop_control "main(): Int64 {
    let i: RefInt64 = refInt(0); let total: RefInt64 = refInt(0);
    while(true){
        let done: Bool = eq(!i,3); if(done){break;}else{}
        let next: Int64 = add(!i,1); assign(i,next);
        while(true){let incremented: Int64 = add(!total,2); assign(total,incremented); break;}
        let skip: Bool = eq(next,2); if(skip){continue;}else{}
        let incremented: Int64 = add(!total,1); assign(total,incremented);
    }
    return !total;
}" 8)

run_case(loop_return "main(): Int64 {
    while(true){if(true){return 7;}else{break;}} return 8;
}" 7)

run_case(target_keywords "func class(this: Int64): Int64 {
    let private: Int64 = add(this,1); return private;
}
main(): Int64 { let match: Int64 = class(4); return match; }" 5)

run_case(later_names "func first(): Int64 { let second: Int64 = 3; return second; }
func second(): Int64 { return 4; }
main(): Int64 { let a: Int64 = first(); let b: Int64 = second();
    let sum: Int64 = add(a,b); return sum; }" 7)

run_case(sibling_scopes "main(): Int64 {
    if(true){let same: Int64 = -001; return same;}else{let same: Int64 = 2; return same;}
}" -1)
run_case(minimum "main(): Int64 { return -0009223372036854775808; }" -9223372036854775808)
run_case(maximum "main(): Int64 { let r: RefInt64 = refInt(9223372036854775807); return ! r; }" 9223372036854775807)
run_case(zero "main(): Int64 { return -0000000000; }" 0)
run_case(unreachable "main(): Int64 { return 1; let unused: Int64 = 2; return unused; }" 1)
run_case(empty "main(): Int64 {}" THROWS)
run_case(fallthrough "func maybe(flag: Bool): RefInt64 {
    if(flag){let r: RefInt64 = refInt(1); return r;}else{}
}
main(): Int64 { let r: RefInt64 = maybe(false); return !r; }" THROWS)

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
