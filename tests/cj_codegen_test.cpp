#include "cj_codegen.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

int main() {
    CHECK(typed::generate_cangjie("main(): Int64 { return 5; }") ==
          "main(): Int64 {\n    return 5\n}\n");
    const std::string source =
        "func increment(value: Int64): Int64 { let next: Int64 = add(value,1); return next; } "
        "main(): Int64 { let result: Int64 = increment(-0004); return result; }";
    const std::string expected =
        "func typed_fn_increment(typed_var_value: Int64): Int64 {\n"
        "    let typed_var_next: Int64 = (typed_var_value + 1)\n"
        "    return typed_var_next\n"
        "}\n\n"
        "main(): Int64 {\n"
        "    let typed_var_result: Int64 = typed_fn_increment(-4)\n"
        "    return typed_var_result\n"
        "}\n";
    CHECK(typed::generate_cangjie(source) == expected);
    CHECK(typed::generate_cangjie(source) == typed::generate_cangjie(source));

    const auto variables = typed::generate_cangjie(
        "main(): Int64 { var r: Int64 = 1; let copy: Int64 = r; "
        "var flag: Bool = true; r = -2; flag = false; return copy; }");
    CHECK(variables);
    CHECK(variables->find("class Typed") == std::string::npos);
    CHECK(variables->find("var typed_var_r: Int64 = 1") != std::string::npos);
    CHECK(variables->find("let typed_var_copy: Int64 = typed_var_r") != std::string::npos);
    CHECK(variables->find("typed_var_r = -2") != std::string::npos);
    CHECK(variables->find("var typed_var_flag: Bool = true") != std::string::npos);
    CHECK(variables->find("typed_var_flag = false") != std::string::npos);
    CHECK(variables->find("return typed_var_copy") != std::string::npos);
    CHECK(variables->find(":=") == std::string::npos);

    CHECK(typed::generate_cangjie("main(): Int64 { return -0000; }") ==
          "main(): Int64 {\n    return 0\n}\n");
    CHECK(typed::generate_cangjie("main(): Int64 {}")->find("exited without returning") != std::string::npos);
    CHECK(typed::generate_cangjie("main(): Int64 { if(true){return 1;}else{return 2;} }")
          ->find("exited without returning") == std::string::npos);
    for (const auto invalid : {
        "", "main(): Int64 {", "main(): Int64 { return true; }",
        "main(): Int64 { let x: Int64 = 1+2; return x; }",
        "main(): Int64 { let x: Int64 = add(1,mul(2,3)); return x; }",
        "main(): Int64 { var r: Int64 = 0; r:=1; return r; }",
        "main(): Int64 { var r: Int64 = 0; r = true; return r; }",
        "main(): Int64 { break; }",
        "main(): Int64 { if(true){let x: Int64 = 0;}else{} return x; }",
        "func a(): Int64 { let r: Int64 = b(); return r; } func b(): Int64 {return 1;} main(): Int64 {}"
    }) CHECK(!typed::generate_cangjie(invalid));
    const auto collections = typed::generate_cangjie(
        "main(): Int64 { let empty: ListInt64 = emptyList(); "
        "let first: ListInt64 = listPush(empty, 4); var items: ListInt64 = first; "
        "let copy: ListInt64 = items; let second: ListInt64 = listPush(items, 5); items = second; "
        "let n: Int64 = listLen(copy); let text: String = \"a b\"; "
        "let letter: String = strAt(text, 0); let same: Bool = strEq(letter, \"a\"); "
        "if (same) { return n; } else { return 0; } }");
    CHECK(collections);
    CHECK(collections->find("import std.collection.ArrayList") != std::string::npos);
    CHECK(collections->find("typed_rt_list_push") != std::string::npos);
    CHECK(collections->find("let typed_var_copy: ArrayList<Int64> = typed_var_items.clone()") != std::string::npos);
    CHECK(collections->find("typed_var_items = typed_var_second.clone()") != std::string::npos);
    CHECK(collections->find("typed_rt_str_at(typed_var_text, 0)") != std::string::npos);
    CHECK(collections->find("\"a b\"") != std::string::npos);

    std::puts("Cangjie generator tests passed");
}
