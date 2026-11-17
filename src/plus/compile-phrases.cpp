/*
 * SPDX-FileCopyrightText: 2026 siygle
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Build tool: data/plus-phrases.txt -> plus-phrases.bin (memory-mapped by
 * PhraseIndex at run time).
 *
 *   chewing-plus-compile-phrases IN.txt OUT.bin
 */
#include "PlusPhrases.h"

#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s IN.txt OUT.bin\n", argv[0]);
        return 2;
    }
    chewing_plus::PhraseIndex index;
    size_t n = index.load(argv[1]);
    if (n == 0 || index.compiled()) {
        std::fprintf(stderr, "%s: no phrases in %s\n", argv[0], argv[1]);
        return 1;
    }
    index.finalize();
    if (!index.writeCompiled(argv[2])) {
        std::fprintf(stderr, "%s: cannot write %s\n", argv[0], argv[2]);
        return 1;
    }
    std::printf("%zu phrases -> %s\n", n, argv[2]);
    return 0;
}
