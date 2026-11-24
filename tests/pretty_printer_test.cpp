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
    formatted("func choose(flag:Bool):Int64{if(flag){return 1;}else{return 2;}}"
              "main():Int64{let value:Int64=choose(true);return value;}",
              "func choose(flag: Bool): Int64 {\n"
              "    if (flag) {\n"
              "        return 1;\n"
              "    } else {\n"
              "        return 2;\n"
              "    }\n}\n"
              "main(): Int64 {\n"
              "    let value: Int64 = choose(true);\n"
              "    return value;\n}\n");
    formatted("func increment(x:Int64):Int64{let y:Int64=x+1;return y;}"
              "func answer():Int64{let x:Int64=increment(4);return x;}"
              "main():Int64{let result:Int64=answer();return result;}",
              "func increment(x: Int64): Int64 {\n"
              "    let y: Int64 = x + 1;\n"
              "    return y;\n}\n"
              "func answer(): Int64 {\n"
              "    let x: Int64 = increment(4);\n"
              "    return x;\n}\n"
              "main(): Int64 {\n"
              "    let result: Int64 = answer();\n"
              "    return result;\n}\n");
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
    formatted("main():Int64{var x:Int64=5;let next:Int64=x+1;x=next;return x;}",
              "main(): Int64 {\n"
              "    var x: Int64 = 5;\n"
              "    let next: Int64 = x + 1;\n"
              "    x = next;\n"
              "    return x;\n}\n");
    formatted("main():Int64{let x:Int64=2-1;let y:Int64=x--1;"
              "let z:Int64=y*-9223372036854775808;return z;}",
              "main(): Int64 {\n"
              "    let x: Int64 = 2 - 1;\n"
              "    let y: Int64 = x - -1;\n"
              "    let z: Int64 = y * -9223372036854775808;\n"
              "    return z;\n}\n");
    formatted("\tmain():Int64{var f:Bool=true;while(f){"
              "if(f){f=false;}else{}}return 0;}\r\n",
              "main(): Int64 {\n"
              "    var f: Bool = true;\n"
              "    while (f) {\n"
              "        if (f) {\n"
              "            f = false;\n"
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
    formatted("main():Int64{let e:ListInt64=emptyList();let n:Int64=e[0];"
              "let t:String=\"ab\";let c:String=t[0];return n;}",
              "main(): Int64 {\n"
              "    let e: ListInt64 = emptyList();\n"
              "    let n: Int64 = e[0];\n"
              "    let t: String = \"ab\";\n"
              "    let c: String = t[0];\n"
              "    return n;\n}\n");
    for (auto invalid : { "", "main(): Int64 {", "main(): Int64 { return true; }",
                          "main(): Int64 { let x: Int64 = 0; x = 1; return x; }",
                          "main(): Int64 { var x: Int64 = 0; x: =1; return x; }",
                          "main(): Int64 { var x: Int64 = 0; x:=1; return x; }",
                          "main(): Int64 { let x: Int64 = 1 + 2 * 3; return x; }",
                          "main(): Int64 { break; return 0; }",
                          "func bad(): Bool { return 0; } main(): Int64 { return 0; }",
                          "main(): Int64 { return !true; }" }) {
        CHECK(!typed::format_program(invalid));
    }
    std::puts("Pretty printer tests passed");
}
