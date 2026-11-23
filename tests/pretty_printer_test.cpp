#include "pretty_printer.h"
#include "typed_prefix.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

static void formatted(std::string_view source, std::string_view expected) {
    const auto result = typed::format_program(source);
    CHECK(result.has_value());
    if (*result != expected) std::fprintf(stderr, "actual:\n%s", result->c_str());
    CHECK(*result == expected);
    typed::Prefix checker;
    CHECK(checker.append(*result));
    CHECK(checker.can_end());
    CHECK(typed::format_program(*result) == result);
}

int main() {
    formatted("main():Int64{while(true){if(false){continue;}else{break;}}return 0;}",
              "main(): Int64 {\n"
              "    while (true) {\n"
              "        if (false) {\n"
              "            continue;\n"
              "        } else {\n"
              "            break;\n"
              "        }\n"
              "    }\n"
              "    return 0;\n}\n");
    formatted("main ( ) : Int64 { return 0 ; }",
              "main(): Int64 {\n    return 0;\n}\n");
    formatted("main():Int64{let x:RefInt64=refInt(5);x:=! x+1;return !x;}",
              "main(): Int64 {\n"
              "    let x: RefInt64 = refInt(5);\n"
              "    x := !x + 1;\n"
              "    return !x;\n}\n");
    formatted("main():Int64{let x:Int64=2-1;let y:Int64=x--1;"
              "let z:Int64=mul(y,-9223372036854775808);return z;}",
              "main(): Int64 {\n"
              "    let x: Int64 = 2 - 1;\n"
              "    let y: Int64 = x - -1;\n"
              "    let z: Int64 = mul(y, -9223372036854775808);\n"
              "    return z;\n}\n");
    formatted("\tmain():Int64{let f:RefBool=refBool(true);while(!f){"
              "if(!f){f:=false;}else{}}return 0;}\r\n",
              "main(): Int64 {\n"
              "    let f: RefBool = refBool(true);\n"
              "    while (!f) {\n"
              "        if (!f) {\n"
              "            f := false;\n"
              "        } else {\n"
              "        }\n"
              "    }\n"
              "    return 0;\n}\n");
    formatted("main():Int64{let b:Bool=2==1;let c:Bool=b||false;"
              "let d:Bool=c&&true;return -0001;}",
              "main(): Int64 {\n"
              "    let b: Bool = 2 == 1;\n"
              "    let c: Bool = b || false;\n"
              "    let d: Bool = c && true;\n"
              "    return -0001;\n}\n");
    for (auto invalid : { "", "main(): Int64 {", "main(): Int64 { return true; }",
                          "main(): Int64 { var x: Int64 = 0; return x; }",
                          "main(): Int64 { let x: RefInt64 = refInt(0); x: =1; return !x; }",
                          "main(): Int64 { break; return 0; }",
                          "main(): Int64 { if(true){continue;}else{} return 0; }" }) {
        CHECK(!typed::format_program(invalid));
    }
}
