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
    complete("main(): Int64 { let x: Int64 = 2 + 3; return x; }");
    complete("main(): Int64 { var x: Int64 = 2; let next: Int64 = x + 3; x = next; return x; }");
    complete("main(): Int64 { var x: Int64 = 2; x = -3; return x; }");
    complete("main(): Int64 { var r: Int64 = 0; var ready: Bool = true; "
             "ready = false; r = r; return r; }");
    complete("main(): Int64 { let x: Int64 = -9223372036854775808; return x; }");
    complete("main(): Int64 { let x: Int64 = 9223372036854775807; return x; }");
    complete("main(): Int64 { let x: Int64 = 5; let same: Bool = x == 5; if (same) { let y: Int64 = x * 2; } else { } return x; }");
    complete("main(): Int64 { let x: Int64 = 3; let small: Bool = x < 5; "
             "let yes: Bool = small && true; return x; }");
    complete("main(): Int64 { let x: Int64 = 2 - 1; let y: Int64 = x - -1; "
             "let same: Bool = true && false; let either: Bool = same || true; "
             "let opposite: Bool = !either; return y; }");
    complete("main(): Int64 { let name: Bool = true; let number: Int64 = 2; "
             "let compared: Bool = number == 1; return number; }");
    complete("main(): Int64 { let limit: Int64 = 5; var i: Int64 = 0; "
             "var a: Int64 = 0; var b: Int64 = 1; "
             "var running: Bool = true; while (running) { "
             "let atLimit: Bool = i == limit; if (atLimit) { running = false; } else { "
             "let next: Int64 = a + b; a = b; b = next; "
             "let nextIndex: Int64 = i + 1; i = nextIndex; } } return a; }");
    complete("main(): Int64 { var flag: Bool = true; "
             "while (flag) { flag = false; } return 0; }");
    complete("main(): Int64 { var cell: Int64 = 7; "
             "let old: Int64 = cell; let next: Int64 = old + 4; "
             "cell = next; let result: Int64 = cell; return result; }");
    complete("main(): Int64 { var original: Int64 = 0; "
             "let copy: Int64 = original; original = 9; return copy; }");
    complete("main(): Int64 { var flag: Bool = true; "
             "let b: Bool = flag && false; if (flag) { flag = b; } else {} return 0; }");
    complete("main(): Int64 { let flag: Bool = true; if (flag) { "
             "let local: Int64 = 2; } else { let other: Bool = false; } return 0; }");
    complete("main(): Int64 { while(true){break;} return 0; }");
    complete("main(): Int64 { var flag: Bool = true; "
             "while(flag){flag = false; continue;} return 0; }");
    complete("main(): Int64 { while(true){if(true){break;}else{continue;}} return 0; }");
    complete("main(): Int64 { while(true){while(false){continue;break;} "
             "if(false){continue;}else{break;}} return 0; }");
    complete("main(): Int64 { var breakfast: Int64 = 0; "
             "let continued: Int64 = 1; while(true){breakfast = continued; break;} return breakfast; }");
    complete("main(): Int64 { var i: Int64 = 0; var a: Int64 = 0; "
             "var b: Int64 = 1; let done: Bool = false; while(true){if(done){break;}else{let next: Int64 = a + b; a = b; b = next; "
             "let nextIndex: Int64 = i + 1; i = nextIndex;}} return a; }");
    complete("main(): Int64 { var x: Int64 = 1; x = x + 1; return x; }");

    rejected("main(): Int64 { ", "break;", "return 0; }");
    rejected("main(): Int64 { ", "continue;", "return 0; }");
    rejected("main(): Int64 { if(true){", "break;", "}else{} return 0; }");
    rejected("main(): Int64 { if(true){}else{", "continue;", "} return 0; }");
    rejected("main(): Int64 { while(true){break;} ", "continue;", "return 0; }");
    rejected("main(): Int64 { while(true){break ", "}", ";} return 0; }");
    rejected("main(): Int64 { while(true){continue ", "true", ";} return 0; }");
    rejected("main(): Int64 { while(true){let local: Int64 = 1; break;} return ", "local;", "0; }");
    rejected("main(): Int64 { let ", "break ", "breakfast: Int64 = 0; return breakfast; }");
    rejected("main(): Int64 { let ", "continue ", "continued: Int64 = 0; return continued; }");
    rejected("main(): Int64 { var b: Int64 = 0; let breakfast: Int64 = 1; ",
             "breakfast ", "b = 2; return b; }");

    rejected("main(): Int64 { let name: Bool = true; let number: Int64 = 2; return n",
             "ame ", "umber; }");
    rejected("main(): Int64 { let name: Bool = true; let number: Int64 = 2; let x: Bool = n",
             "z", "ame; return 0; }");
    rejected("main(): Int64 { let x: Int64 = 1 + ",
             "false ", "2; return x; }");
    rejected("main(): Int64 { var r: Int64 = 0; r = ",
             "true ", "1; return r; }");
    rejected("main(): Int64 { let x: Int64 = 1; ",
             "x ", "return x; }");
    rejected("main(): Int64 { var x: Int64 = 1; x = ",
             "true ", "2; return x; }");
    rejected("main(): Int64 { let x: Int64 = ",
             "9223372036854775808", "9223372036854775807; return x; }");
    // Operand types stay fixed by the operator. Nesting follows precedence.
    rejected("main(): Int64 { let x: Int64 = 1 + 2 * ", "true", "3; return x; }");
    rejected("main(): Int64 { let b: Bool = 2 == ", "true", "1; return 0; }");
    rejected("main(): Int64 { let b: Bool = 2 < ", "true", "3; return 0; }");
    rejected("main(): Int64 { let x: Int64 = 2 + ", "true", "1; return x; }");
    rejected("main(): Int64 { let x: Int64 = -", "9223372036854775809",
             "9223372036854775808; return x; }");
    rejected("main(): Int64 { while (true) { var local: Int64 = 0; } ",
             "local", "return 0; }");
    rejected("main(): Int64 { let x: Int64 = 1; let ",
             "x ", "xx: Int64 = 2; return x; }");
    rejected("main(): Int64 { if (true) { let x: Int64 = 1; } else { } return ",
             "x ", "0; }");
    rejected("main(): Int64 { let items: ListInt64 = emptyList(); if (",
             "}", "true) { } else { } return 0; }");
    rejected("main(): Int64 { ", "fn ", "return 0; }");

    rejected("main(): Int64 { let x: Int64 = 1; ", "x = ", "return x; }");
    rejected("main(): Int64 { var x: Int64 = 0; ", "write(", "return x; }");
    rejected("main(): Int64 { var x: Int64 = 0; let v: Int64 = ",
             "readInt(", "x; return v; }");
    rejected("main(): Int64 { let x: Int64 = 0; return ", "!", "x; }");
    rejected("main(): Int64 { var r: Bool = true; return ", "!", "0; }");
    rejected("main(): Int64 { var r: Int64 = 0; ", "r:=5;", "r = 5; return r; }");
    rejected("main(): Int64 { var r: Int64 = 0; ", "r: =5;", "r = 5; return r; }");
    rejected("main(): Int64 { ", "assign(", "return 0; }");
    rejected("main(): Int64 { let x: Int64 = 0; ", "assign(", "return x; }");
    rejected("main(): Int64 { var r: Int64 = 0; let x: Int64 = 1; ",
             "x = ", "r = x; return r; }");
    rejected("main(): Int64 { var flag: Bool = true; flag = ", "0;", "false; return 0; }");
    rejected("main(): Int64 { var r: Int64 = 0; r", ")", " = 1; return r; }");
    rejected("main(): Int64 { var r: Int64 = 0; r = 1", ",", "; return r; }");
    rejected("main(): Int64 { var r: Int64 = 0; r = 1", "}", "; return r; }");
    rejected("main(): Int64 { let ", "assign ", "assignment: Int64 = 0; return assignment; }");
    rejected("main(): Int64 { var r: Int64 = 0; let x: Int64 = ", "assign(", "0; return x; }");
    rejected("main(): Int64 { while(true){var r: Int64 = 0;} ", "r = ", "return 0; }");
    rejected("main(): Int64 { var n: Int64 = 0; let name: Bool = true; "
             "let x: Int64 = n", "a", "; return x; }");
    rejected("main(): Int64 { let empty: ListInt64 = emptyList(); let n: Int64 = empty[",
             "true", "0]; return n; }");
    rejected("main(): Int64 { let empty: ListInt64 = emptyList(); let n: Int64 = empty[0]",
             "[", "; return n; }");

    Prefix p;
    CHECK(p.append("main(): Int64 { return 0; }"));
    CHECK(p.can_end());
    CHECK(!p.can_append(" more"));

    Prefix multi;
    CHECK(multi.append("main ( ) : Int64 { return "));
    CHECK(multi.can_append("123 ; }"));
    CHECK(multi.append("123 ; }"));
    CHECK(multi.can_end());

    // Rejection mid-expression must keep the prefix so another candidate can finish.
    Prefix split;
    CHECK(split.append("main(): Int64 { let b: Bool = 2 =="));
    CHECK(!split.can_append("+"));
    CHECK(!split.append("+"));
    const auto suffix = split.completion();
    CHECK(suffix.has_value());
    Prefix finished = split;
    CHECK(finished.append(*suffix));
    CHECK(finished.can_end());
    CHECK(split.append(" 1; return 0; }"));
    CHECK(split.can_end());

    for (auto seed : {
        "main(): Int64 { var r: Int64 = 0; r",
        "main(): Int64 { var r: Int64 = 0; r ",
        "main(): Int64 { var r: Int64 = 0; r =",
        "main(): Int64 { var r: Int64 = 0; r = ",
        "main(): Int64 { var r: Int64 = 0; r = -",
        "main(): Int64 { var r: Int64 = 0; r = 0",
        "main(): Int64 { var f: Bool = true; f =",
        "main(): Int64 { var r: Bool = false; let b: Bool = ",
        "main(): Int64 { var r: Int64 = 0; let b: Bool = r ==",
        "main(): Int64 { let b: Bool = ",
        "main(): Int64 { let x: Int64 = -",
        "main(): Int64 { return -",
        "main(): Int64 { var r: Bool = true; while (",
        "main(): Int64 { while(true){br",
        "main(): Int64 { while(true){continu",
        "main(): Int64 { while(true){break ",
        "main(): Int64 { while(true){if(true){continue ",
        "main(): Int64 { while(true){while(true){break;}"
    }) {
        Prefix base;
        CHECK(base.append(seed));
        for (char ch = 32; ch < 127; ++ch) {
            Prefix candidate = base;
            if (!candidate.append(std::string_view(&ch, 1))) continue;
            const auto rest = candidate.completion();
            CHECK(rest.has_value());
            CHECK(candidate.append(*rest));
            CHECK(candidate.can_end());
        }
    }

    {
        Prefix ok;
        CHECK(ok.append("main(): Int64 { return 0; }"));
        CHECK(!ok.diagnostic());
        CHECK(ok.can_end());
    }

    const auto expect_error = [](std::string_view source, size_t line, size_t column,
                                 std::string_view message_part, std::string_view continuation) {
        Prefix checker;
        CHECK(!checker.append(source));
        CHECK(checker.diagnostic());
        CHECK(checker.diagnostic()->line == line);
        CHECK(checker.diagnostic()->column == column);
        CHECK(checker.diagnostic()->message.find(message_part) != std::string::npos);
        CHECK(checker.append(continuation));
        CHECK(checker.can_end());
        CHECK(!checker.diagnostic());
    };

    expect_error("main(): Int64 { return false; }", 1, 24, "expected Int64, found Bool 'false'",
                 "main(): Int64 { return 0; }");
    expect_error("main(): Int64 { let x: Int64 = 1; let x: Int64 = 2; return x; }", 1, 39,
                 "name 'x' is already defined",
                 "main(): Int64 { let x: Int64 = 1; let y: Int64 = 2; return x; }");
    expect_error("main(): Int64 { break; return 0; }", 1, 17, "'break' is only allowed inside a loop",
                 "main(): Int64 { return 0; }");
    expect_error("main(): Int64 { let x: Int64 = 9223372036854775808; return x; }", 1, 32,
                 "Int64 literal is out of range",
                 "main(): Int64 { let x: Int64 = 9223372036854775807; return x; }");
    expect_error("main(): Int64 { let x: Int64 = 1; x = 2; return x; }", 1, 35,
                 "cannot assign to immutable 'x'",
                 "main(): Int64 { let x: Int64 = 1; return x; }");
    complete("main(): Int64 { let empty: ListInt64 = emptyList(); "
             "let grown: ListInt64 = listPush(empty, 4); "
             "let n: Int64 = listLen(grown); let item: Int64 = grown[0]; "
             "let sum: Int64 = n + item; return sum; }");
    complete("main(): Int64 { let text: String = \"a b\"; let letter: String = text[0]; "
             "let joined: String = text + letter; let same: Bool = letter == \"a\"; "
             "let length: Int64 = strLen(joined); if (same) { return length; } else { return 0; } }");
    rejected("main(): Int64 { let text: String = ", "1", "\"ok\"; return 0; }");
    rejected("main(): Int64 { let items: ListInt64 = emptyList(); let n: Int64 = ", "true",
             "listLen(items); return n; }");

    complete("func id<T>(value: T): T { return value; } "
             "main(): Int64 { let n: Int64 = id<Int64>(4); return n; }");
    complete("func apply(f: (Int64) -> Int64, value: Int64): Int64 { return f(value); } "
             "main(): Int64 { let inc: (Int64) -> Int64 = fun(x: Int64): Int64 => x + 1; "
             "let n: Int64 = apply(inc, 4); return n; }");
    rejected("func id<T>(value: T): T { let bad: Int64 = ", "value",
             "0; return value; } main(): Int64 { return id<Int64>(1); }");
    complete("main(): Int64 { let n: Int64 = 1 + 2 * 3; if (n + 1 == 8) { return n; } else { return 0; } }");
    complete("main(): Int64 { let xs: ListInt64 = emptyList(); "
             "let n: Int64 = listLen(xs); if (n == 0) { return n; } else { return xs[0]; } }");

    Prefix unfinished;
    CHECK(unfinished.append("main(): Int64 {"));
    CHECK(!unfinished.diagnostic());
    CHECK(!unfinished.can_end());
    const auto end = unfinished.end_diagnostic();
    CHECK(end.message.find("unexpected end of input") != std::string::npos);
    CHECK(end.message.find("statement") != std::string::npos);

}
