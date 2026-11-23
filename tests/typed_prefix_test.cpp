#include "typed_prefix.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

using typed::Prefix;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

static void complete(std::string_view program) {
    Prefix checker;
    for (size_t i = 0; i < program.size(); ++i) {
        CHECK(checker.can_append(program.substr(i, 1)));
        CHECK(checker.append(program.substr(i, 1)));
        CHECK(checker.can_end() == (i + 1 == program.size()));
        const auto suffix = checker.completion();
        CHECK(suffix.has_value());
        Prefix finished = checker;
        CHECK(finished.append(*suffix));
        CHECK(finished.can_end());
    }
}

static void rejected(std::string_view valid_prefix, std::string_view invalid_piece,
                     std::string_view valid_continuation) {
    Prefix checker;
    CHECK(checker.append(valid_prefix));
    if (checker.can_append(invalid_piece))
        std::fprintf(stderr, "unexpected accepted piece after prefix: %.*s | %.*s\n",
            static_cast<int>(valid_prefix.size()), valid_prefix.data(),
            static_cast<int>(invalid_piece.size()), invalid_piece.data());
    CHECK(!checker.can_append(invalid_piece));
    CHECK(!checker.append(invalid_piece));
    CHECK(checker.append(valid_continuation));
    CHECK(checker.can_end());
}

int main() {
    complete("main ( ) : Int64 { return 0 ; }");
    complete("main(): Int64 { return 0; }");
    complete("main(): Int64 { let x: Int64 = add(2, 3); return x; }");
    complete("main(): Int64 { var x: Int64 = 2; x = add(x, 3); return x; }");
    complete("main(): Int64 { var x: Int64 = 2; x=x+3; return x; }");
    complete("main(): Int64 { let x: Int64 = -9223372036854775808; return x; }");
    complete("main(): Int64 { let x: Int64 = 9223372036854775807; return x; }");
    complete("main(): Int64 { let x: Int64 = 5; let done: Bool = x==5; "
             "if (done) { let y: Int64 = x*2; } else { } return x; }");
    complete("main(): Int64 { let x: Int64 = 3; let small: Bool = x<5; "
             "let yes: Bool = small&&true; return x; }");
    complete("main(): Int64 { let x: Int64 = 2-1; let y: Int64 = x--1; "
             "let same: Bool = true==false; let either: Bool = same||true; return y; }");
    complete("main(): Int64 { let name: Bool = true; let number: Int64 = 2; "
             "let compared: Bool = number == 1; return number; }");
    complete("main(): Int64 { let limit: Int64 = 5; var i: Int64 = 0; "
             "var a: Int64 = 0; var b: Int64 = 1; var running: Bool = true; "
             "while (running) { let done: Bool = i == limit; if (done) { "
             "running = false; } else { let next: Int64 = a + b; a = b; "
             "b = next; i = i + 1; } } return a; }");
    complete("main(): Int64 { var flag: Bool = true; while (flag) { flag = false; } return 0; }");
    complete("main(): Int64 { let cell: RefInt64 = refInt(7); "
             "let old: Int64 = readInt(cell); let next: Int64 = add(old, 4); "
             "write(cell, next); let result: Int64 = readInt(cell); return result; }");
    complete("main(): Int64 { let original: RefInt64 = refInt(0); "
             "let alias: RefInt64 = original; write(alias, 9); "
             "let value: Int64 = readInt(original); return value; }");
    complete("main(): Int64 { let flag: Bool = true; if (flag) { "
             "let local: Int64 = 2; } else { let other: Bool = false; } return 0; }");

    rejected("main(): Int64 { let name: Bool = true; let number: Int64 = 2; return n",
             "ame ", "umber; }");
    rejected("main(): Int64 { let name: Bool = true; let number: Int64 = 2; let x: Bool = n",
             "z", "ame; return 0; }");
    rejected("main(): Int64 { let x: Int64 = add(1, ",
             "false ", "2); return x; }");
    rejected("main(): Int64 { let x: Int64 = add(1 ",
             ") ", ", 2); return x; }");
    rejected("main(): Int64 { let r: RefInt64 = refInt(0); write(r, ",
             "true ", "1); return 0; }");
    rejected("main(): Int64 { let x: Int64 = 1; ",
             "x ", "return x; }");
    rejected("main(): Int64 { var x: Int64 = 1; x = ",
             "true ", "2; return x; }");
    rejected("main(): Int64 { let x: Int64 = ",
             "9223372036854775808", "9223372036854775807; return x; }");
    rejected("main(): Int64 { let x: Int64 = 2 ",
             "< ", "+ 3; return x; }");
    rejected("main(): Int64 { let done: Bool = 2 < ",
             "true ", "3; return 0; }");
    rejected("main(): Int64 { let done: Bool = 2 ",
             ";", "== 1; return 0; }");
    rejected("main(): Int64 { let done: Bool = 2 ",
             "+", "< 1; return 0; }");
    rejected("main(): Int64 { let x: Int64 = 2 + ",
             "true", "1; return x; }");
    rejected("main(): Int64 { let x: Int64 = -", "9223372036854775809",
             "9223372036854775808; return x; }");
    rejected("main(): Int64 { while (true) { var local: Int64 = 0; } ",
             "local", "return 0; }");
    rejected("main(): Int64 { let x: Int64 = 1; let ",
             "x ", "xx: Int64 = 2; return x; }");
    rejected("main(): Int64 { if (true) { let x: Int64 = 1; } else { } return ",
             "x ", "0; }");
    rejected("main(): Int64 { if (", "1 ", "true) { } else { } return 0; }");
    rejected("main(): Int64 { ", "fn ", "return 0; }");

    Prefix p;
    CHECK(p.append("main(): Int64 { return 0; }"));
    CHECK(p.can_end());
    CHECK(!p.can_append(" more"));

    Prefix multi;
    CHECK(multi.append("main ( ) : Int64 { return "));
    CHECK(multi.can_append("123 ; }"));
    CHECK(multi.append("123 ; }"));
    CHECK(multi.can_end());

    // A model token can end halfway through an operator. Rejection must keep
    // that partial operator so a later candidate can finish the same prefix.
    Prefix split;
    CHECK(split.append("main(): Int64 { let b: Bool = 2="));
    CHECK(!split.can_append("+"));
    CHECK(!split.append("+"));
    const auto suffix = split.completion();
    CHECK(suffix.has_value());
    Prefix finished = split;
    CHECK(finished.append(*suffix));
    CHECK(finished.can_end());
    CHECK(split.append("=1; return 0; }"));
    CHECK(split.can_end());
}
