#include "typed_prefix.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

static void complete(std::string_view program) {
    typed::Prefix whole;
    CHECK(whole.append(program));
    CHECK(whole.can_end());
    typed::Prefix checker;
    for (size_t i = 0; i < program.size(); ++i) {
        CHECK(checker.append(program.substr(i, 1)));
        CHECK(checker.can_end() == (i + 1 == program.size()));
        const auto suffix = checker.completion();
        if (!suffix) std::fprintf(stderr, "no completion after: %.*s\n", static_cast<int>(i + 1), program.data());
        CHECK(suffix);
        typed::Prefix finished = checker;
        CHECK(finished.append(*suffix));
        CHECK(finished.can_end());
    }
}

static void rejected(std::string_view prefix, std::string_view invalid, std::string_view continuation) {
    typed::Prefix checker;
    CHECK(checker.append(prefix));
    CHECK(!checker.can_append(invalid));
    CHECK(!checker.append(invalid));
    CHECK(checker.append(continuation));
    CHECK(checker.can_end());
}

int main() {
    const std::string identity = "func identity(x: Int64): Int64 { return x; } ";
    const std::string pair = "func combine(a: Int64, b: Bool): Bool { let result: Bool = a == 0; return result; } ";
    const std::string extract = "func extract(r: RefInt64): Int64 { return !r; } ";
    complete(identity + "main(): Int64 { let value: Int64 = identity(5); return value; }");
    complete("func answer(): Int64 { return 42; } main(): Int64 { let x: Int64 = answer(); return x; }");
    complete(pair + "main(): Int64 { let same: Bool = combine(0, true); if(same){}else{} return 0; }");
    complete("func create(x: Int64): RefInt64 { let cell: RefInt64 = refInt(x); return cell; } "
             "main(): Int64 { let r: RefInt64 = create(5); return !r; }");
    complete("func create(): RefBool { let r: RefBool = refBool(true); return r; } "
             "main(): Int64 { let r: RefBool = create(); if(!r){}else{} return 0; }");
    complete("func mutate(a: Int64, b: Bool, r: RefInt64, flag: RefBool): Int64 { r:=a; flag:=b; return !r; } "
             "main(): Int64 { let cell: RefInt64 = refInt(0); let flag: RefBool = refBool(false); "
             "let value: Int64 = mutate(5, true, cell, flag); cell:=mutate(6,false,cell,flag); return value; }");
    complete(identity + "func reuse(x: Bool): Bool { return x; } "
             "main(): Int64 { let x: Int64 = identity(5); let flag: Bool = reuse(true); return x; }");
    complete("func factorial(n: Int64): Int64 { let result: RefInt64 = refInt(1); let base: Bool = n < 2; "
             "if(base){}else{let previous: Int64 = n - 1; let product: Int64 = factorial(previous); "
             "result := n * product;} return !result; } "
             "main(): Int64 { let result: Int64 = factorial(5); return result; }");
    complete("func loop(flag: RefBool): Int64 { while(!flag){if(!flag){flag:=false;continue;}else{break;}} return 1; } "
             "main(): Int64 { let flag: RefBool = refBool(true); let value: Int64 = loop(flag); return value; }");
    complete("main(): Int64 { if(true){return 1;}else{return 2;} }");
    complete("main(): Int64 { while(true){if(false){return 1;}else{continue;} break;} return 2; }");
    complete("main(): Int64 { return 1; let value: Int64 = 2; return value; }");
    complete("func factorial(n: Int64): Int64 { let base: Bool = n < 2; if(base){return 1;}else{"
             "let previous: Int64 = n - 1; let partial: Int64 = factorial(previous); "
             "let product: Int64 = n * partial; return product;} } "
             "main(): Int64 { let result: Int64 = factorial(5); return result; }");
    complete("func flag(x: Bool): Bool { if(x){return true;}else{while(x){return false;}} return true; } "
             "main(): Int64 { let value: Bool = flag(false); return 0; }");
    complete("func choose(x: Bool, first: RefInt64, second: RefInt64): RefInt64 { "
             "if(x){return first;}else{return second;} } main(): Int64 { let a: RefInt64 = refInt(1); "
             "let b: RefInt64 = refInt(2); let result: RefInt64 = choose(true,a,b); return !result; }");
    complete("func choose(x: RefBool): RefBool { while(!x){return x;} return x; } "
             "main(): Int64 { let flag: RefBool = refBool(true); let result: RefBool = choose(flag); return 0; }");
    // Return presence and reachability are intentionally not analyzed.
    complete("func optional(x: Bool): Int64 { if(x){return 1;}else{} } main(): Int64 {}");

    rejected("main(): Int64 { if(true){return ", "false;", "1;}else{return 2;} }");
    rejected("func flag(): Bool { while(true){return ", "1;", "false;} } main(): Int64 {return 0;}");
    rejected("main(): Int64 { return 1; return ", "true;", "2; }");
    rejected("func choose(x: RefInt64): RefInt64 { if(true){return ", "!x;", "x;}else{return x;} } main(): Int64 {}");
    rejected("main(): Int64 { if(true){let local: Int64 = 1; return local;}else{return 0;} return ", "local;", "0; }");
    rejected("main(): Int64 { if(true){return 1 ", "}", ";}else{return 2;} }");

    rejected(identity + "main(): Int64 { let result: Int64 = identity(", "true", "5); return result; }");
    rejected(identity + "main(): Int64 { let result: Int64 = identity(", ")", "5); return result; }");
    rejected(identity + "main(): Int64 { let result: Int64 = identity(5", ",", "); return result; }");
    rejected(pair + "main(): Int64 { let flag: Bool = combine(0,", "1", "true); return 0; }");
    rejected(identity + "main(): Int64 { let flag: Bool = ", "identity(", "true; return 0; }");
    rejected(extract + "main(): Int64 { let result: Int64 = ", "extract(", "0; return result; }");
    rejected(identity + "main(): Int64 { let result: Int64 = identity(", "identity(", "5); return result; }");
    rejected(identity + "main(): Int64 { let result: Int64 = identity(5)", "+", "; return result; }");
    rejected(identity + "main(): Int64 { return ", "identity(", "0; }");
    rejected(identity + "main(): Int64 { return ", "x;", "0; }");
    rejected(identity + "main(): Int64 { let ", "identity ", "value: Int64 = 0; return value; }");
    rejected(identity + "func ", "identity ", "other(): Int64 { return 0; } main(): Int64 { return 0; }");
    rejected("func ", "add ", "addition(): Int64 { return 0; } main(): Int64 { return 0; }");
    rejected("func ", "main ", "helper(): Int64 { return 0; } main(): Int64 { return 0; }");
    rejected("func ", "readInt ", "reader(): Int64 { return 0; } main(): Int64 { return 0; }");
    rejected("func same(x: Int64, ", "x ", "y: Int64): Int64 { return x; } main(): Int64 { return 0; }");
    rejected("func same(", "same ", "value: Int64): Int64 { return value; } main(): Int64 { return 0; }");
    rejected("func same(x: Int64, ", ")", "y: Bool): Int64 { return x; } main(): Int64 { return 0; }");
    rejected("func flag(): Bool { return ", "1;", "false; } main(): Int64 { return 0; }");
    rejected("func create(): RefInt64 { ", "return ", "let r: RefInt64 = refInt(0); return r; } main(): Int64 { return 0; }");
    rejected("func first(): Int64 { let x: Int64 = ", "second(", "0; return x; } main(): Int64 { return 0; }");
    rejected("func test(): Int64 { ", "break;", "return 0; } main(): Int64 { return 0; }");
    rejected("main(): Int64 { ", "func ", "return 0; }");
    rejected("main(): Int64 { let ", "func ", "functional: Int64 = 0; return functional; }");

    // Long user signatures must not exhaust the completion budget mid-call.
    std::string many = "func many(";
    for (int i = 0; i < 40; ++i) many += (i ? "," : "") + std::string("p") + std::to_string(i) + ": Int64";
    many += "): Int64 { return p0; } main(): Int64 { let result: Int64 = many(";
    for (int i = 0; i < 40; ++i) many += i ? ",0" : "0";
    complete(many + "); return result; }");

    for (const auto & seed : {
        std::string("fu"), std::string("func new"), std::string("func test("),
        std::string("func test(x: Ref"), std::string("func test(x: Int64,"),
        std::string("func create(): RefBool {"), std::string("func create(): RefInt64 { let r:"),
        identity + "main(): Int64 { let result: Int64 = ident",
        pair + "main(): Int64 { let result: Bool = combine(0,",
        std::string("main(): Int64 { if(true){ret"),
        std::string("func flag(): Bool { while(true){return "),
        std::string("func choose(x: RefInt64): RefInt64 { if(true){ret")
    }) {
        typed::Prefix base;
        CHECK(base.append(seed));
        for (char ch = 32; ch < 127; ++ch) {
            auto candidate = base;
            if (!candidate.append(std::string_view(&ch, 1))) continue;
            const auto suffix = candidate.completion();
            CHECK(suffix);
            CHECK(candidate.append(*suffix));
            CHECK(candidate.can_end());
        }
    }
    typed::Prefix incomplete;
    CHECK(incomplete.append(identity));
    CHECK(!incomplete.can_end());
    CHECK(incomplete.append("main(): Int64 { return 0; }"));
    CHECK(!incomplete.append("func later(): Int64 { return 0; }"));
    std::puts("Function tests passed");
}
