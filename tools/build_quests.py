#!/usr/bin/env python3
"""Compile content/quests/*.quest into static C++ tables for the engine.

Usage:
  build_quests.py [--check] <quest-file-or-dir>... [output.cpp]

With --check, only validates (no output). Otherwise the last argument is
the generated .cpp path. Any validation error fails with a message, so
invalid content breaks the build by construction.

Generated tables reference engine/quest.h structs and expose:
  const l3d::QuestBank l3d::content_quests();
"""
import re
import sys
from pathlib import Path

OBJECTIVES = {"TALK", "REACH", "COLLECT", "GIVE", "USE", "INSPECT"}
STATES = {"ACTIVE": 1, "COMPLETED": 2, "CLAIMED": 3}
MAX_OBJECTIVES, MAX_PREREQS = 8, 2
MAX_TITLE, MAX_DESC = 128, 256


class Fail(Exception):
    pass


def esc(text):
    if '"' in text:
        raise Fail("double quote not allowed in text: %r" % text)
    return text.replace("\\", "\\\\")


def parse_cond(tokens, where):
    """At most one condition per objective; returns dict or None."""
    cond = None

    def take(name):
        nonlocal cond
        if name in tokens:
            if cond is not None:
                raise Fail(where + ": at most one condition per objective")
            cond = tokens.pop(name)
            return True
        return False

    if take("if_item"):
        m = re.fullmatch(r"(\d+):(\d+)", cond)
        if not m:
            raise Fail(where + ": bad if_item (want id:count)")
        return {"t": 1, "item": int(m.group(1)), "count": int(m.group(2))}
    if take("if_flag"):
        if not re.fullmatch(r"\d+", cond) or int(cond) > 31:
            raise Fail(where + ": bad if_flag (want 0..31)")
        return {"t": 2, "flag": int(cond)}
    if take("if_counter"):
        m = re.fullmatch(r"(\d+):(\d+)", cond)
        if not m or int(m.group(1)) > 7:
            raise Fail(where + ": bad if_counter (want idx:threshold)")
        return {"t": 3, "counter": int(m.group(1)), "threshold": int(m.group(2))}
    if take("if_level"):
        if not re.fullmatch(r"\d+", cond):
            raise Fail(where + ": bad if_level")
        return {"t": 4, "level": int(cond)}
    return None


def parse_quest_file(path):
    quests, cur = [], None
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        where = "%s:%d" % (path, lineno)
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        head, _, rest = line.partition(" ")
        if head == "quest":
            if cur is not None:
                quests.append(cur)
            m = re.fullmatch(r"(\d+)", rest.strip())
            if not m or not (1 <= int(m.group(1)) <= 65534):
                raise Fail(where + ": bad quest id")
            cur = {"id": int(m.group(1)), "title": None, "desc": None,
                   "prereqs": [], "objectives": []}
        elif cur is None:
            raise Fail(where + ": directive outside quest")
        elif head == "title":
            if cur["title"] is not None or not (1 <= len(rest.strip()) <= MAX_TITLE):
                raise Fail(where + ": bad title")
            cur["title"] = rest.strip()
        elif head == "description":
            if cur["desc"] is not None or not (1 <= len(rest.strip()) <= MAX_DESC):
                raise Fail(where + ": bad description")
            cur["desc"] = rest.strip()
        elif head == "prereq":
            m = re.fullmatch(r"(\d+) (ACTIVE|COMPLETED|CLAIMED)", rest.strip())
            if not m:
                raise Fail(where + ": bad prereq")
            if len(cur["prereqs"]) >= MAX_PREREQS:
                raise Fail(where + ": too many prereqs")
            cur["prereqs"].append({"q": int(m.group(1)), "s": STATES[m.group(2)]})
        elif head == "objective":
            parts = rest.strip().split()
            if not parts or parts[0] not in OBJECTIVES:
                raise Fail(where + ": bad objective type")
            if len(cur["objectives"]) >= MAX_OBJECTIVES:
                raise Fail(where + ": too many objectives")
            typ = parts[0]
            tokens = {}
            for kv in parts[1:]:
                if "=" not in kv:
                    raise Fail(where + ": bad objective token %r" % kv)
                k, v = kv.split("=", 1)
                tokens[k] = v
            try:
                tag = int(tokens.pop("tag", "0"))
                item = int(tokens.pop("item", "0"))
                count = int(tokens.pop("count", "1"))
                npc = int(tokens.pop("npc", "0"))
            except ValueError:
                raise Fail(where + ": bad numeric objective param")
            cond = parse_cond(tokens, where)
            if tokens:
                raise Fail(where + ": unknown objective keys %r" % sorted(tokens))
            if typ in ("TALK", "REACH", "INSPECT") and tag == 0:
                raise Fail(where + ": %s needs tag=" % typ)
            if typ in ("COLLECT", "GIVE") and (item == 0 or count == 0):
                raise Fail(where + ": %s needs item= and count=" % typ)
            if typ == "GIVE" and npc == 0:
                raise Fail(where + ": GIVE needs npc=")
            if typ == "USE" and item == 0:
                raise Fail(where + ": USE needs item=")
            cur["objectives"].append({"t": typ, "tag": tag, "item": item,
                                      "count": count, "npc": npc, "cond": cond})
        else:
            raise Fail(where + ": unknown directive")
    if cur is not None:
        quests.append(cur)
    for q in quests:
        if q["title"] is None or q["desc"] is None:
            raise Fail("%s: quest %d missing title/description" % (path, q["id"]))
        if not q["objectives"]:
            raise Fail("%s: quest %d has no objectives" % (path, q["id"]))
    return quests


def validate(quests, origin):
    seen = set()
    for q in quests:
        if q["id"] in seen:
            raise Fail("%s: duplicate quest id %d" % (origin, q["id"]))
        seen.add(q["id"])


OTYPE = {"TALK": 1, "REACH": 2, "COLLECT": 3, "GIVE": 4, "USE": 5, "INSPECT": 6}


def cond_emit(c):
    base = {"t": 0, "item": 0, "count": 0, "flag": 0,
            "counter": 0, "threshold": 0, "level": 0}
    if c is not None:
        base["t"] = c["t"]
        for k in ("item", "count", "flag", "counter", "threshold", "level"):
            if k in c:
                base[k] = c[k]
    return "{%d, %d, %d, %d, %d, %d, %d}" % (
        base["t"], base["item"], base["count"], base["flag"],
        base["counter"], base["threshold"], base["level"])


def emit(quests, out_path):
    parts = ['#include "quest.h"', "", "namespace l3d {", "namespace {", ""]
    for q in quests:
        parts.append('static const char title_%d[] = "%s";' % (q["id"], esc(q["title"])))
        parts.append('static const char desc_%d[] = "%s";' % (q["id"], esc(q["desc"])))
    parts.append("static const QuestDef defs[] = {")
    for q in quests:
        pre = ["{%d, %d}" % (p["q"], p["s"]) for p in q["prereqs"]]
        while len(pre) < MAX_PREREQS:
            pre.append("{65535, 2}")
        obs = []
        for o in q["objectives"]:
            obs.append("{%d, %d, %d, %d, %d, %s}" % (
                OTYPE[o["t"]], o["tag"], o["item"], o["count"], o["npc"],
                cond_emit(o["cond"])))
        while len(obs) < MAX_OBJECTIVES:
            obs.append("{0, 0, 0, 1, 0, {0, 0, 0, 0, 0, 0, 0}}")
        parts.append("    {%d, title_%d, desc_%d, %d, {%s}, %d, {%s}}," % (
            q["id"], q["id"], q["id"], len(q["prereqs"]), ", ".join(pre),
            len(q["objectives"]), ", ".join(obs)))
    parts.append("};")
    parts.append("")
    parts.append("} // namespace")
    parts.append("")
    parts.append("const QuestBank content_quests() {")
    parts.append("    QuestBank b{defs, sizeof(defs) / sizeof(defs[0])};")
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
            files += sorted(p.glob("*.quest"))
        elif p.is_file():
            files.append(p)
        else:
            raise Fail("not found: %s" % raw)
    if not files:
        raise Fail("no .quest inputs")
    return files


def main(argv):
    check = "--check" in argv
    argv = [a for a in argv if a != "--check"]
    try:
        if check:
            if not argv:
                print("usage: build_quests.py --check <quest-or-dir>...",
                      file=sys.stderr)
                return 2
            all_q = []
            for f in collect(argv):
                all_q += parse_quest_file(f)
            validate(all_q, "content")
            print("quest content OK: %d quest(s)" % len(all_q))
            return 0
        if len(argv) < 2:
            print("usage: build_quests.py <quest-or-dir>... <output.cpp>",
                  file=sys.stderr)
            return 2
        *ins, out = argv
        all_q = []
        for f in collect(ins):
            all_q += parse_quest_file(f)
        validate(all_q, "content")
        emit(all_q, Path(out))
        print("generated %s: %d quest(s)" % (out, len(all_q)))
        return 0
    except Fail as ex:
        print("build_quests.py: error: %s" % ex, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
