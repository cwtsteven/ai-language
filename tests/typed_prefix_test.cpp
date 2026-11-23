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
    complete("main(): Int64 { let x: RefInt64 = refInt(2); x := add(!x, 3); return !x; }");
    complete("main(): Int64 { let x: RefInt64 = refInt(2); x:=!x+3; return ! x; }");
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
    complete("main(): Int64 { let limit: Int64 = 5; let i: RefInt64 = refInt(0); "
             "let a: RefInt64 = refInt(0); let b: RefInt64 = refInt(1); "
             "let running: RefBool = refBool(true); while (!running) { "
             "let done: Bool = !i == limit; if (done) { running := false; } else { "
             "let next: Int64 = !a + !b; a := !b; b := next; i := !i + 1; } } return !a; }");
    complete("main(): Int64 { let flag: RefBool = refBool(true); "
             "while (!flag) { flag := false; } return 0; }");
    complete("main(): Int64 { let cell: RefInt64 = refInt(7); "
             "let old: Int64 = !cell; let next: Int64 = old + 4; "
             "cell := next; let result: Int64 = !cell; return result; }");
    complete("main(): Int64 { let original: RefInt64 = refInt(0); "
             "let alias: RefInt64 = original; alias := 9; return !original; }");
    complete("main(): Int64 { let flag: RefBool = refBool(true); "
             "let b: Bool = !flag && false; if (!flag) { flag := b; } else {} return 0; }");
    complete("main(): Int64 { let flag: Bool = true; if (flag) { "
             "let local: Int64 = 2; } else { let other: Bool = false; } return 0; }");
    complete("main(): Int64 { while(true){break;} return 0; }");
    complete("main(): Int64 { let flag: RefBool = refBool(true); "
             "while(!flag){flag:=false;continue;} return 0; }");
    complete("main(): Int64 { while(true){if(true){break;}else{continue;}} return 0; }");
    complete("main(): Int64 { while(true){while(false){continue;break;} "
             "if(false){continue;}else{break;}} return 0; }");
    complete("main(): Int64 { let breakfast: RefInt64 = refInt(0); "
             "let continued: Int64 = 1; while(true){breakfast:=continued;break;} return !breakfast; }");
    complete("main(): Int64 { let i: RefInt64 = refInt(0); let a: RefInt64 = refInt(0); "
             "let b: RefInt64 = refInt(1); while(true){let done: Bool = !i == 5; "
             "if(done){break;}else{let next: Int64 = !a + !b; a:=!b; b:=next; i:=!i+1;}} return !a; }");

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
    rejected("main(): Int64 { let b: RefInt64 = refInt(0); let breakfast: Int64 = 1; b",
             "reak;", ":=2; return !b; }");

    rejected("main(): Int64 { let name: Bool = true; let number: Int64 = 2; return n",
             "ame ", "umber; }");
    rejected("main(): Int64 { let name: Bool = true; let number: Int64 = 2; let x: Bool = n",
             "z", "ame; return 0; }");
    rejected("main(): Int64 { let x: Int64 = add(1, ",
             "false ", "2); return x; }");
    rejected("main(): Int64 { let x: Int64 = add(1 ",
             ") ", ", 2); return x; }");
    rejected("main(): Int64 { let r: RefInt64 = refInt(0); r := ",
             "true ", "1; return 0; }");
    rejected("main(): Int64 { let x: Int64 = 1; ",
             "x ", "return x; }");
    rejected("main(): Int64 { let x: RefInt64 = refInt(1); x := ",
             "true ", "2; return !x; }");
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
    rejected("main(): Int64 { while (true) { let local: RefInt64 = refInt(0); } ",
             "local", "return 0; }");
    rejected("main(): Int64 { let x: Int64 = 1; let ",
             "x ", "xx: Int64 = 2; return x; }");
    rejected("main(): Int64 { if (true) { let x: Int64 = 1; } else { } return ",
             "x ", "0; }");
    rejected("main(): Int64 { if (", "1 ", "true) { } else { } return 0; }");
    rejected("main(): Int64 { ", "fn ", "return 0; }");

    rejected("main(): Int64 { ", "var ", "return 0; }");
    rejected("main(): Int64 { let x: RefInt64 = refInt(0); ", "write(", "return 0; }");
    rejected("main(): Int64 { let x: RefInt64 = refInt(0); let v: Int64 = ",
             "readInt(", "!x; return v; }");
    rejected("main(): Int64 { let x: Int64 = 1; return ", "!", "x; }");
    rejected("main(): Int64 { let r: RefBool = refBool(true); return ", "!", "0; }");
    rejected("main(): Int64 { let r: RefInt64 = refInt(0); r", "=", ":=5; return !r; }");
    rejected("main(): Int64 { let r: RefInt64 = refInt(0); r:", "+", "=5; return !r; }");
    rejected("main(): Int64 { let n: RefInt64 = refInt(0); let name: RefBool = refBool(true); "
             "let x: Int64 = !n", "a", "; return x; }");

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
    // All accepted one-byte candidates from boundary states must retain a
    // completion, including whitespace after ! and half of := or ==.
    for (auto seed : {
        "main(): Int64 { let r: RefInt64 = refInt(0); r",
        "main(): Int64 { let r: RefInt64 = refInt(0); r:",
        "main(): Int64 { let r: RefInt64 = refInt(0); r:=!",
        "main(): Int64 { let r: RefInt64 = refInt(0); let b: Bool = !",
        "main(): Int64 { let r: RefInt64 = refInt(0); let b: Bool = !r=",
        "main(): Int64 { let r: RefBool = refBool(true); while (!",
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

}
