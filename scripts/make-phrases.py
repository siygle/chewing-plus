#!/usr/bin/env python3
"""Build Plus suggestion data from libchewing-data's dict/chewing/tsi.csv,
optionally merged with McBopomofo (MIT) and curated modern phrase lists.

    python3 scripts/make-phrases.py path/to/tsi.csv > data/plus-phrases.txt
    python3 scripts/make-phrases.py --chars path/to/tsi.csv > data/plus-chars.txt
    python3 scripts/make-phrases.py path/to/tsi.csv \\
        --mcbopomofo path/to/BPMFMappings.txt \\
        --mcbopomofo-occ path/to/phrase.occ \\
        --extra data/plus-phrases-modern.txt > data/plus-phrases.txt

Phrases: one line per phrase of two or more characters: phrase, frequency,
and the first Bopomofo symbol of each syllable (the abbreviation), tab
separated. Highest frequency wins when a phrase has several readings.

--chars: one line per single character and toneless reading (好 ㄏㄠ), with
the highest frequency among its toned readings. Characters never used
(frequency 0) are left out. Used to list likely characters for a partly
typed reading (ㄋ -> 你 年 您).

Sorted so the files diff cleanly.
"""
import argparse
import sys


TONES = "ˊˇˋ˙ˉ"
# Particle characters that form noisy 2-char fragments in unigram LM tables.
NOISY_PARTICLES = set("的了著嗎呢吧啊呀哦喔嘛")


def is_bopomofo_symbol(ch):
    return "\u3105" <= ch <= "\u3129"


def is_han(ch):
    code = ord(ch)
    return (
        0x4E00 <= code <= 0x9FFF
        or 0x3400 <= code <= 0x4DBF
        or 0xF900 <= code <= 0xFAFF
        or 0x20000 <= code <= 0x2EBEF
    )


def initials_from_reading(reading):
    """Extract first Bopomofo symbol per syllable (space-separated or compact)."""
    reading = reading.strip()
    if not reading:
        return ""
    if " " in reading:
        syls = [s for s in reading.split(" ") if s]
        abbr = "".join(s[0] for s in syls if s)
    else:
        abbr = "".join(ch for ch in reading if ch not in TONES)
    if abbr and all(is_bopomofo_symbol(c) for c in abbr):
        return abbr
    return ""


def rows(path):
    version = ""
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("# dc:identifier,"):
                version = line.split(",")[1]
            if line.startswith("#"):
                continue
            parts = line.rstrip("\n").split(",")
            if len(parts) < 3:
                continue
            try:
                freq = int(parts[1])
            except ValueError:
                continue
            yield version, parts[0], freq, parts[2]


def load_mcbopomofo_occ(path):
    occ = {}
    if not path:
        return occ
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            parts = line.strip().split()
            if len(parts) == 2 and parts[1].isdigit():
                occ[parts[0]] = int(parts[1])
    return occ


def merge_mcbopomofo(best, mappings_path, occ_path):
    """Merge McBopomofo phrases not already in `best`, filtering unigram
    particle fragments and scaling frequencies conservatively."""
    occ = load_mcbopomofo_occ(occ_path)
    base_words = set(best.keys())
    added = 0
    with open(mappings_path, encoding="utf-8") as fh:
        for line in fh:
            if not line or line.startswith("#"):
                continue
            parts = line.strip().split()
            if len(parts) < 3:
                continue
            word = parts[0]
            syls = parts[1:]
            if word in best or len(word) < 2 or len(word) > 8:
                continue
            if len(syls) != len(word) or "\t" in word:
                continue
            if not all(is_han(c) for c in word):
                continue
            # Filter noisy LM fragments: starting with 的, 2-char particle
            # combinations, or "<existing_phrase>的".
            if word[0] == "的":
                continue
            if len(word) == 2 and (
                word[0] in NOISY_PARTICLES or word[-1] in NOISY_PARTICLES
            ):
                continue
            if word.endswith("的") and word[:-1] in base_words:
                continue
            abbr = "".join(s[0] for s in syls if s)
            if len(abbr) != len(word) or not all(
                is_bopomofo_symbol(c) for c in abbr
            ):
                continue
            raw_occ = occ.get(word, 0)
            # Median tsi/occ ratio on shared phrases is ~3.0; scale
            # conservatively (x2, capped at 1500) so curated tsi.csv top
            # entries remain stable while modern words still rank well.
            freq = min(raw_occ * 2, 1500) if raw_occ > 0 else 1
            best[word] = (freq, abbr)
            added += 1
    return added


def merge_extra(best, extra_path):
    """Merge a curated tab-separated phrase file:
    phrase<TAB>freq<TAB>bopomofo_or_initials"""
    added = 0
    updated = 0
    with open(extra_path, encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 3:
                continue
            word = parts[0].strip()
            try:
                freq = int(parts[1].strip())
            except ValueError:
                continue
            abbr = initials_from_reading(parts[2])
            if len(word) < 2 or len(abbr) != len(word):
                continue
            if word not in best:
                best[word] = (freq, abbr)
                added += 1
            elif freq > best[word][0]:
                best[word] = (freq, abbr)
                updated += 1
    return added, updated


def header(out, version, what, extra_sources):
    out.write(
        "# "
        + what
        + f" from libchewing-data dict/chewing/tsi.csv ({version}),"
        + " LGPL-2.1-or-later, libchewing Core Team.\n"
    )
    for src in extra_sources:
        out.write(f"# Merged with {src}.\n")
    out.write("# Generated by scripts/make-phrases.py; do not edit.\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--chars",
        action="store_true",
        help="Build single-character toneless reading index instead of phrases",
    )
    parser.add_argument(
        "--mcbopomofo",
        metavar="PATH",
        help="Optional McBopomofo BPMFMappings.txt (MIT) to merge new phrases",
    )
    parser.add_argument(
        "--mcbopomofo-occ",
        metavar="PATH",
        help="Optional McBopomofo phrase.occ occurrence table for --mcbopomofo",
    )
    parser.add_argument(
        "--extra",
        action="append",
        default=[],
        metavar="PATH",
        help="Optional curated TSV phrase list(s) to merge",
    )
    parser.add_argument("tsi_csv", help="Path to libchewing-data tsi.csv")
    args = parser.parse_args()

    best = {}
    version = ""
    for version, word, freq, phone in rows(args.tsi_csv):
        syllables = phone.split(" ")
        if args.chars:
            if len(word) != 1 or len(syllables) != 1 or freq <= 0:
                continue
            reading = "".join(ch for ch in syllables[0] if ch not in TONES)
            # Bopomofo symbols themselves (ㄋ read as ㄋ) are not characters
            # anyone picks from the list.
            if not reading or "\t" in word or "\u3105" <= word <= "\u3129":
                continue
            key = (word, reading)
            if freq > best.get(key, -1):
                best[key] = freq
            continue
        if len(word) < 2 or len(syllables) != len(word) or "\t" in word:
            continue
        abbr = "".join(s[0] for s in syllables if s)
        if len(abbr) != len(word):
            continue
        if word not in best or freq > best[word][0]:
            best[word] = (freq, abbr)

    out = sys.stdout
    if args.chars:
        header(out, version, "Characters", [])
        out.write("# char<TAB>frequency<TAB>reading without tone\n")
        for (word, reading) in sorted(best):
            out.write(f"{word}\t{best[(word, reading)]}\t{reading}\n")
        return

    base_count = len(best)
    extra_sources = []
    if args.mcbopomofo:
        mcb_added = merge_mcbopomofo(best, args.mcbopomofo, args.mcbopomofo_occ)
        extra_sources.append(
            "McBopomofo BPMFMappings.txt (MIT, data/LICENSE.mcbopomofo)"
        )
        print(
            f"make-phrases: base={base_count}, +mcbopomofo={mcb_added}",
            file=sys.stderr,
        )
    for extra_path in args.extra:
        e_added, e_updated = merge_extra(best, extra_path)
        extra_sources.append(f"{extra_path} (LGPL-2.1-or-later)")
        print(
            f"make-phrases: +{extra_path} added={e_added}, updated={e_updated}",
            file=sys.stderr,
        )
    print(f"make-phrases: total={len(best)}", file=sys.stderr)

    header(out, version, "Phrases", extra_sources)
    out.write("# phrase<TAB>frequency<TAB>first Bopomofo of each syllable\n")
    for word in sorted(best):
        freq, abbr = best[word]
        out.write(f"{word}\t{freq}\t{abbr}\n")


if __name__ == "__main__":
    main()
