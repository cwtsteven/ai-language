#include "cj_codegen.h"
#include "typed_prefix.h"

#include <cstdio>
#include <string>

// Translate one complete typed program from stdin. Model generation does this
// itself; benchmarks and runtime tests use this for checked-in typed fixtures.
int main() {
    std::string source;
    char buffer[4096];
    size_t size = 0;
    while ((size = std::fread(buffer, 1, sizeof(buffer), stdin)) != 0)
        source.append(buffer, size);
    if (std::ferror(stdin)) {
        std::fputs("could not read stdin\n", stderr);
        return 2;
    }
    typed::Prefix checker;
    const bool viable = checker.append(source);
    if (!viable || !checker.can_end()) {
        if (!viable && checker.diagnostic())
            typed::print_diagnostic(stderr, "<stdin>", source, *checker.diagnostic());
        else if (viable)
            typed::print_diagnostic(stderr, "<stdin>", source, checker.end_diagnostic());
        else
            std::fputs("translation requires a complete, well-typed program\n", stderr);
        return 2;
    }
    const auto generated = typed::generate_cangjie(source);
    if (!generated) {
        std::fputs("translation requires a complete, well-typed program\n", stderr);
        return 2;
    }
    std::fwrite(generated->data(), 1, generated->size(), stdout);
    return std::ferror(stdout) ? 1 : 0;
}
