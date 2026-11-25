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
        "func increment(value: Int64): Int64 { let next: Int64 = value + 1; return next; } "
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
        "main(): Int64 { let x: Int64 = 1 + true; return x; }",
        "main(): Int64 { var r: Int64 = 0; r:=1; return r; }",
        "main(): Int64 { var r: Int64 = 0; r = true; return r; }",
        "main(): Int64 { break; }",
        "main(): Int64 { if(true){let x: Int64 = 0;}else{} return x; }",
        "func a(): Int64 { let r: Int64 = b(); return r; } func b(): Int64 {return 1;} main(): Int64 {}"
    }) CHECK(!typed::generate_cangjie(invalid));
    const auto collections = typed::generate_cangjie(
        "main(): Int64 { let empty: List<Int64> = List<Int64>(); "
        "let first: List<Int64> = empty.add(4); var items: List<Int64> = first; "
        "let copy: List<Int64> = items; let second: List<Int64> = items.add(5); items = second; "
        "let n: Int64 = copy.size; let text: String = \"a b\"; "
        "let letter: String = text[0]; let same: Bool = letter == \"a\"; "
        "if (same) { return n; } else { return 0; } }");
    CHECK(collections);
    CHECK(collections->find("import std.collection.ArrayList") != std::string::npos);
    CHECK(collections->find("typed_rt_list_add_Int64") != std::string::npos);
    CHECK(collections->find("let typed_var_copy: ArrayList<Int64> = typed_var_items.clone()") != std::string::npos);
    CHECK(collections->find("typed_var_items = typed_var_second.clone()") != std::string::npos);
    CHECK(collections->find("typed_rt_str_at(typed_var_text, 0)") != std::string::npos);
    CHECK(collections->find("\"a b\"") != std::string::npos);

    const auto infix_condition = typed::generate_cangjie(
        "main(): Int64 { var x: Int64 = 1; let done: Bool = x == 0; if (done) { return 1; } else { return x + 1; } }");
    CHECK(infix_condition);
    CHECK(infix_condition->find("if (typed_var_done)") != std::string::npos);
    CHECK(infix_condition->find("(typed_var_x + 1)") != std::string::npos);

    const auto nested = typed::generate_cangjie(
        "main(): Int64 { let x: Int64 = 1 + 2 * 3; if (x + 1 == 8) { return x; } else { return 0; } }");
    CHECK(nested);
    CHECK(nested->find("(1 + (2 * 3))") != std::string::npos);
    CHECK(nested->find("((typed_var_x + 1) == 8)") != std::string::npos);

    const auto generic = typed::generate_cangjie(
        "func id<T>(value: T): T { return value; } "
        "main(): Int64 { let n: Int64 = id<Int64>(4); return n; }");
    CHECK(generic);
    CHECK(generic->find("func typed_fn_id<T>(typed_var_value: T): T") != std::string::npos);
    CHECK(generic->find("typed_fn_id<Int64>(4)") != std::string::npos);

    const auto higher = typed::generate_cangjie(
        "func apply(f: (Int64) -> Int64, value: Int64): Int64 { return f(value); } "
        "main(): Int64 { let inc: (Int64) -> Int64 = fun(x: Int64): Int64 => x + 1; "
        "let n: Int64 = apply(inc, 4); return n; }");
    CHECK(higher);
    CHECK(higher->find("func typed_fn_apply(typed_var_f: (Int64) -> Int64, typed_var_value: Int64): Int64") !=
          std::string::npos);
    CHECK(higher->find("{typed_var_x: Int64 => (typed_var_x + 1)}") != std::string::npos);
    CHECK(higher->find("typed_fn_apply(typed_var_inc, 4)") != std::string::npos);

    const auto point = typed::generate_cangjie(
        "struct Point { x: Int64; y: Int64; "
        "func dx(d: Int64): Int64 { let n: Int64 = x + d; return n; } } "
        "main(): Int64 { let p: Point = Point(1, 2); return p.dx(3); }");
    CHECK(point);
    CHECK(point->find("struct Point {") != std::string::npos);
    CHECK(point->find("init(typed_var_x: Int64, typed_var_y: Int64)") != std::string::npos);
    CHECK(point->find("func dx(typed_var_d: Int64): Int64") != std::string::npos);
    CHECK(point->find("this.typed_var_x") != std::string::npos);
    CHECK(point->find("typed_var_p.dx(3)") != std::string::npos);
    CHECK(point->find("typed_rt_copy_Point") != std::string::npos);

    const auto for_list = typed::generate_cangjie(
        "main(): Int64 { let empty: List<Int64> = List<Int64>(); "
        "let items: List<Int64> = empty.add(4); var total: Int64 = 0; "
        "for (item: Int64 in items) { total = total + item; } return total; }");
    CHECK(for_list);
    CHECK(for_list->find("for (typed_var_item in typed_var_items) {") != std::string::npos);
    CHECK(for_list->find("typed_var_total = (typed_var_total + typed_var_item)") != std::string::npos);

    const auto for_range = typed::generate_cangjie(
        "main(): Int64 { var total: Int64 = 0; "
        "for (i: Int64 in 0 .. 3) { total = total + i; } return total; }");
    CHECK(for_range);
    CHECK(for_range->find("for (typed_var_i in 0..3) {") != std::string::npos);
    CHECK(for_range->find("typed_var_total = (typed_var_total + typed_var_i)") != std::string::npos);

    const auto arithmetic = typed::generate_cangjie(
        "main(): Int64 { let q: Int64 = 7 / 2; let r: Int64 = 7 % 2; "
        "let ok: Bool = q >= r && r != 0; if (ok) { return q; } return r; }");
    CHECK(arithmetic);
    CHECK(arithmetic->find("(7 / 2)") != std::string::npos);
    CHECK(arithmetic->find("(7 % 2)") != std::string::npos);
    CHECK(arithmetic->find("(typed_var_q >= typed_var_r)") != std::string::npos);
    CHECK(arithmetic->find("(typed_var_r != 0)") != std::string::npos);
    CHECK(arithmetic->find("} else {") == std::string::npos);

    const auto with_else = typed::generate_cangjie(
        "main(): Int64 { if (true) { return 1; } else { return 2; } }");
    CHECK(with_else);
    CHECK(with_else->find("} else {") != std::string::npos);

    std::puts("Cangjie generator tests passed");
}
