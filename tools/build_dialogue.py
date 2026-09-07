#!/usr/bin/env python3
"""Compile content/dialogues/*.dlg into static C++ tables for the engine.

Usage:
  build_dialogue.py [--check] <dlg-file-or-dir>... <output.cpp>

With --check, only validates (no output). Without --check the last argument
is the generated .cpp path. Any validation error fails with a message
(human or CI-readable), so invalid content breaks the build by construction.

Generated tables reference engine/dialogue.h structs and expose:
  const l3d::DialogueBank l3d::content_dialogues();
"""
import re
import sys
from pathlib import Path

SPEAKERS = {"npc": 0, "player": 1, "narrator": 2}
LANGS = {"und": 0, "en": 1, "de": 2, "pl": 3, "es": 4, "fr": 5}
CEFRS = {"A1": 1, "A2": 2, "B1": 3, "B2": 4, "C1": 5, "C2": 6}
MAX_NODES, MAX_CHOICES, MAX_TAGS, MAX_TEXT = 16, 4, 4, 192


class Fail(Exception):
    pass


def esc(text):
    if '"' in text:
        raise Fail("double quote not allowed in text: %r" % text)
    out = text.replace("\\", "\\\\").replace("\n", " ")
    return out


def u16list(raw, what, where):
    if not raw:
        return []
    try:
        vals = [int(x) for x in raw.split(",") if x.strip() != ""]
    except ValueError:
        raise Fail("%s: bad %s list %r" % (where, what, raw))
    if any(v < 1 or v > 65534 for v in vals):
        raise Fail("%s: %s ids out of range %r" % (where, what, raw))
    if len(vals) > MAX_TAGS:
        raise Fail("%s: too many %s tags (max %d)" % (where, what, MAX_TAGS))
    return vals


def parse_dlg(path):
    dialogues = []
    cur_d, cur_n, cur_text = None, None, None

    def flush_node():
        nonlocal cur_n, cur_text
        if cur_n is None:
            return
        if cur_text is None:
            raise Fail("%s: node %d has no text" % (path, cur_n["id"]))
        cur_n["text"] = cur_text
        cur_d["nodes"].append(cur_n)
        cur_n, cur_text = None, None

    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        where = "%s:%d" % (path, lineno)
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("dialogue "):
            flush_node()
            if cur_d is not None:
                dialogues.append(cur_d)
            m = re.fullmatch(r"dialogue (\d+) npc=(\d+)", line)
            if not m:
                raise Fail(where + ": bad dialogue header")
            did, npc = int(m.group(1)), int(m.group(2))
            if not (1 <= did <= 65534):
                raise Fail(where + ": dialogue id out of range")
            cur_d = {"id": did, "npc": npc, "nodes": []}
        elif line.startswith("node "):
            if cur_d is None:
                raise Fail(where + ": node outside dialogue")
            flush_node()
            m = re.fullmatch(
                r"node (\d+) speaker=(\w+) lang=(\w+) cefr=(\w+)"
                r"(?: vocab=([\d,]+))?(?: grammar=([\d,]+))?"
                r"(?: cond=(\d+))?(?: effect=(\d+))?", line)
            if not m:
                raise Fail(where + ": bad node line")
            nid = int(m.group(1))
            if not (1 <= nid <= 65534):
                raise Fail(where + ": node id out of range")
            if m.group(2) not in SPEAKERS:
                raise Fail(where + ": bad speaker %r" % m.group(2))
            if m.group(3) not in LANGS:
                raise Fail(where + ": bad lang %r" % m.group(3))
            if m.group(4) not in CEFRS:
                raise Fail(where + ": bad cefr %r" % m.group(4))
            cond = int(m.group(7) or 0)
            effect = int(m.group(8) or 0)
            if cond > 255 or effect > 255:
                raise Fail(where + ": cond/effect out of range")
            cur_n = {"id": nid, "speaker": SPEAKERS[m.group(2)],
                     "lang": LANGS[m.group(3)], "cefr": CEFRS[m.group(4)],
                     "vocab": u16list(m.group(5), "vocab", where),
                     "grammar": u16list(m.group(6), "grammar", where),
                     "cond": cond, "effect": effect, "choices": []}
        elif line.startswith("text "):
            if cur_n is None or cur_text is not None:
                raise Fail(where + ": stray text line")
            text = line[5:].strip()
            if not (1 <= len(text) <= MAX_TEXT):
                raise Fail(where + ": text length out of range")
            cur_text = text
        elif line.startswith("choice "):
            if cur_n is None or cur_text is None:
                raise Fail(where + ": choice outside node text")
            m = re.fullmatch(r'choice "([^"]+)" -> (\d+|END)', line)
            if not m:
                raise Fail(where + ": bad choice line")
            if len(cur_n["choices"]) >= MAX_CHOICES:
                raise Fail(where + ": too many choices")
            nxt = m.group(2)
            cur_n["choices"].append(
                {"text": m.group(1), "next": 65535 if nxt == "END" else int(nxt)})
        else:
            raise Fail(where + ": unknown directive")
    flush_node()
    if cur_d is not None:
        dialogues.append(cur_d)
    if not dialogues:
        raise Fail("%s: no dialogues" % path)
    return dialogues


def validate(dialogues, origin):
    seen = set()
    for d in dialogues:
        if d["id"] in seen:
            raise Fail("%s: duplicate dialogue id %d" % (origin, d["id"]))
        seen.add(d["id"])
        if not d["nodes"]:
            raise Fail("%s: dialogue %d has no nodes" % (origin, d["id"]))
        if len(d["nodes"]) > MAX_NODES:
            raise Fail("%s: dialogue %d too many nodes" % (origin, d["id"]))
        ids = {n["id"] for n in d["nodes"]}
        if len(ids) != len(d["nodes"]):
            raise Fail("%s: dialogue %d duplicate node id" % (origin, d["id"]))
        for n in d["nodes"]:
            for c in n["choices"]:
                if c["next"] != 65535 and c["next"] not in ids:
                    raise Fail("%s: dialogue %d node %d dangling choice -> %d"
                               % (origin, d["id"], n["id"], c["next"]))


def emit(dialogues, out_path):
    parts = ['#include "dialogue.h"', "",
             "namespace l3d {", "namespace {", ""]
    for di, d in enumerate(dialogues):
        for ni, n in enumerate(d["nodes"]):
            parts.append('static const char t_%d_%d[] = "%s";'
                         % (d["id"], n["id"], esc(n["text"])))
            for ci, c in enumerate(n["choices"]):
                parts.append('static const char c_%d_%d_%d[] = "%s";'
                             % (d["id"], n["id"], ci, esc(c["text"])))
        parts.append("static const DialogueNode nodes_%d[] = {" % d["id"])
        for n in d["nodes"]:
            ch = ["{c_%d_%d_%d, %d}" % (d["id"], n["id"], ci, c["next"])
                  for ci, c in enumerate(n["choices"])]
            while len(ch) < MAX_CHOICES:
                ch.append("{nullptr, 65535}")
            v = ["%d" % x for x in n["vocab"]]
            while len(v) < MAX_TAGS:
                v.append("0")
            g = ["%d" % x for x in n["grammar"]]
            while len(g) < MAX_TAGS:
                g.append("0")
            parts.append(
                "    {%d, %d, %d, {%s}, t_%d_%d, {%d, %d, {%s}, %d, {%s}, %d}, %d, %d},"
                % (n["id"], n["speaker"], len(n["choices"]), ", ".join(ch),
                   d["id"], n["id"], n["lang"], n["cefr"], ", ".join(v),
                   len(n["vocab"]), ", ".join(g), len(n["grammar"]),
                   n["cond"], n["effect"]))
        parts.append("};")
    parts.append("static const DialogueDef defs[] = {")
    for d in dialogues:
        parts.append("    {%d, %d, %d, nodes_%d}," % (d["id"], d["npc"], len(d["nodes"]), d["id"]))
    parts.append("};")
    parts.append("")
    parts.append("} // namespace")
    parts.append("")
    parts.append("const DialogueBank content_dialogues() {")
    parts.append("    DialogueBank b{defs, sizeof(defs) / sizeof(defs[0])};")
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
            files += sorted(p.glob("*.dlg"))
        elif p.is_file():
            files.append(p)
        else:
            raise Fail("not found: %s" % raw)
    if not files:
        raise Fail("no .dlg inputs")
    return files


def main(argv):
    check = "--check" in argv
    argv = [a for a in argv if a != "--check"]
    try:
        if check:
            if not argv:
                print("usage: build_dialogue.py --check <dlg-or-dir>...",
                      file=sys.stderr)
                return 2
            all_d = []
            for f in collect(argv):
                all_d += parse_dlg(f)
            validate(all_d, "content")
            print("dialogue content OK: %d dialogue(s)" % len(all_d))
            return 0
        if len(argv) < 2:
            print("usage: build_dialogue.py <dlg-or-dir>... <output.cpp>",
                  file=sys.stderr)
            return 2
        *ins, out = argv
        all_d = []
        for f in collect(ins):
            all_d += parse_dlg(f)
        validate(all_d, "content")
        emit(all_d, Path(out))
        print("generated %s: %d dialogue(s)" % (out, len(all_d)))
        return 0
    except Fail as ex:
        print("build_dialogue.py: error: %s" % ex, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
