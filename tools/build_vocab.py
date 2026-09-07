#!/usr/bin/env python3
"""Compile content/vocabulary/*.vocab into static C++ tables for the engine.

Usage:
  build_vocab.py [--check] <vocab-file-or-dir>... [output.cpp]

With --check, only validates (no output). Otherwise the last argument is
the generated .cpp path. Any validation error fails with a message, so
invalid content breaks the build by construction.

Generated tables reference engine/language.h structs and expose:
  const l3d::VocabularyBank l3d::content_vocabulary();
"""
import re
import sys
from pathlib import Path

LANGS = {"und": 0, "en": 1, "de": 2, "pl": 3, "es": 4, "fr": 5}
POS = {"noun": 1, "verb": 2, "adj": 3, "adv": 4, "phrase": 5,
       "preposition": 6, "pronoun": 7, "numeral": 8}
CEFRS = {"A1": 1, "A2": 2, "B1": 3, "B2": 4, "C1": 5, "C2": 6}
MAX_LEMMA = 48


class Fail(Exception):
    pass


def esc(text):
    if '"' in text:
        raise Fail("double quote not allowed in lemma: %r" % text)
    return text.replace("\\", "\\\\")


def parse_vocab_file(path):
    entries, cur = [], None
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        where = "%s:%d" % (path, lineno)
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("vocab "):
            if cur is not None:
                entries.append(cur)
            m = re.fullmatch(
                r"vocab (\d+) lang=(\w+) pos=(\w+) cefr=(\w+) cat=([A-Za-z_]+)",
                line)
            if not m:
                raise Fail(where + ": bad vocab line")
            vid = int(m.group(1))
            if not (1 <= vid <= 65534):
                raise Fail(where + ": vocab id out of range")
            for name, table in (("lang", LANGS), ("pos", POS), ("cefr", CEFRS)):
                grp = m.group({"lang": 2, "pos": 3, "cefr": 4}[name])
                if grp not in table:
                    raise Fail(where + ": bad %s %r" % (name, grp))
            cur = {"id": vid, "lang": LANGS[m.group(2)], "pos": POS[m.group(3)],
                   "cefr": CEFRS[m.group(4)], "cat": m.group(5), "lemma": None}
        elif line.startswith("lemma "):
            if cur is None or cur["lemma"] is not None:
                raise Fail(where + ": stray lemma line")
            lemma = line[6:].strip()
            if not (1 <= len(lemma) <= MAX_LEMMA):
                raise Fail(where + ": lemma length out of range")
            cur["lemma"] = lemma
        else:
            raise Fail(where + ": unknown directive")
    if cur is not None:
        entries.append(cur)
    for e in entries:
        if e["lemma"] is None:
            raise Fail("%s: vocab %d has no lemma" % (path, e["id"]))
    return entries


def validate(entries, origin):
    seen = set()
    for e in entries:
        if e["id"] in seen:
            raise Fail("%s: duplicate vocab id %d" % (origin, e["id"]))
        seen.add(e["id"])


def emit(entries, out_path):
    parts = ['#include "language.h"', "", "namespace l3d {", "namespace {", ""]
    for e in entries:
        parts.append('static const char l_%d[] = "%s";' % (e["id"], esc(e["lemma"])))
        parts.append('static const char k_%d[] = "%s";' % (e["id"], e["cat"]))
    parts.append("static const VocabularyEntry defs[] = {")
    for e in entries:
        parts.append("    {%d, l_%d, %d, %d, %d, k_%d}," % (
            e["id"], e["id"], e["lang"], e["pos"], e["cefr"], e["id"]))
    parts.append("};")
    parts.append("")
    parts.append("} // namespace")
    parts.append("")
    parts.append("const VocabularyBank content_vocabulary() {")
    parts.append("    VocabularyBank b{defs, sizeof(defs) / sizeof(defs[0])};")
    parts.append("    return b;")
    parts.append("}")
    parts.append("")
    parts.append("} // namespace l3d")
    out_path.write_text("\n".join(parts), encoding="utf-8")


def collect(inputs):
    files = []
    for raw in inputs:
        p = Path(raw)
        if p.is_dir():
            files += sorted(p.glob("*.vocab"))
        elif p.is_file():
            files.append(p)
        else:
            raise Fail("not found: %s" % raw)
    if not files:
        raise Fail("no .vocab inputs")
    return files


def main(argv):
    check = "--check" in argv
    argv = [a for a in argv if a != "--check"]
    try:
        if check:
            if not argv:
                print("usage: build_vocab.py --check <vocab-or-dir>...",
                      file=sys.stderr)
                return 2
            all_e = []
            for f in collect(argv):
                all_e += parse_vocab_file(f)
            validate(all_e, "content")
            print("vocab content OK: %d entries" % len(all_e))
            return 0
        if len(argv) < 2:
            print("usage: build_vocab.py <vocab-or-dir>... <output.cpp>",
                  file=sys.stderr)
            return 2
        *ins, out = argv
        all_e = []
        for f in collect(ins):
            all_e += parse_vocab_file(f)
        validate(all_e, "content")
        emit(all_e, Path(out))
        print("generated %s: %d entries" % (out, len(all_e)))
        return 0
    except Fail as ex:
        print("build_vocab.py: error: %s" % ex, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
