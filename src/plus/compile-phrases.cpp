/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Build tool: data/plus-phrases.txt -> plus-phrases.bin (memory-mapped by
 * PhraseIndex at run time).
 *
 *   chewing-plus-compile-phrases [--chars] IN.txt OUT.bin
 *
 * --chars: the input is data/plus-chars.txt (one whole reading per entry).
 */
#include "PlusPhrases.h"
#include <cstring>

#include <cstdio>

int main(int argc, char **argv) {
    bool chars = argc == 4 && std::strcmp(argv[1], "--chars") == 0;
    if (argc != 3 && !chars) {
        std::fprintf(stderr, "usage: %s [--chars] IN.txt OUT.bin\n", argv[0]);
        return 2;
    }
    const char *in = argv[argc - 2];
    const char *outPath = argv[argc - 1];
    chewing_plus::PhraseIndex index;
    size_t n = index.load(in, chars);
    if (n == 0 || index.compiled()) {
        std::fprintf(stderr, "%s: no entries in %s\n", argv[0], in);
        return 1;
    }
    index.finalize();
    if (!index.writeCompiled(outPath)) {
        std::fprintf(stderr, "%s: cannot write %s\n", argv[0], outPath);
        return 1;
    }
    std::printf("%zu entries -> %s\n", n, outPath);
    return 0;
}
