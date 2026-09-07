#!/usr/bin/env python3
"""Compile content/items/*.item into static C++ tables for the engine.

Usage:
  build_items.py [--check] <item-file-or-dir>... [output.cpp]

With --check, only validates (no output). Otherwise the last argument is
the generated .cpp path. Any validation error fails with a message, so
invalid content breaks the build by construction.

Generated tables reference engine/item.h structs and expose:
  const l3d::ItemBank l3d::content_items();
"""
import re
import sys
from pathlib import Path

CATEGORIES = {"food": 1, "tool": 2, "document": 3, "key": 4, "ticket": 5,
              "book": 6, "clothing": 7, "currency": 8, "artifact": 9,
              "quest": 10}
MAX_VOCAB, MAX_NAME = 4, 64


class Fail(Exception):
    pass


def esc(text):
    if '"' in text:
        raise Fail("double quote not allowed in name: %r" % text)
    return text.replace("\\", "\\\\")


def parse_item_file(path):
    items, cur = [], None
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        where = "%s:%d" % (path, lineno)
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("item "):
            if cur is not None:
                items.append(cur)
            m = re.fullmatch(
                r"item (\d+) category=(\w+) stackable=([01]) max=(\d+)"
                r" weight=(\d+)(?: vocab=([\d,]+))?", line)
            if not m:
                raise Fail(where + ": bad item line")
            iid = int(m.group(1))
            if not (1 <= iid <= 65534):
                raise Fail(where + ": item id out of range")
            if m.group(2) not in CATEGORIES:
                raise Fail(where + ": bad category %r" % m.group(2))
            stackable = m.group(3) == "1"
            max_stack = int(m.group(4))
            if max_stack < 1 or max_stack > 65534:
                raise Fail(where + ": bad max stack")
            if not stackable and max_stack != 1:
                raise Fail(where + ": non-stackable requires max=1")
            vocab = []
            if m.group(6):
                try:
                    vocab = [int(x) for x in m.group(6).split(",")]
                except ValueError:
                    raise Fail(where + ": bad vocab list")
                if any(v < 1 or v > 65534 for v in vocab):
                    raise Fail(where + ": vocab id out of range")
                if len(vocab) > MAX_VOCAB:
                    raise Fail(where + ": too many vocab tags")
            cur = {"id": iid, "cat": CATEGORIES[m.group(2)],
                   "stack": stackable, "max": max_stack,
                   "weight": int(m.group(5)), "vocab": vocab, "name": None}
        elif line.startswith("name "):
            if cur is None or cur["name"] is not None:
                raise Fail(where + ": stray name line")
            name = line[5:].strip()
            if not (1 <= len(name) <= MAX_NAME):
                raise Fail(where + ": name length out of range")
            cur["name"] = name
        else:
            raise Fail(where + ": unknown directive")
    if cur is not None:
        items.append(cur)
    for it in items:
        if it["name"] is None:
            raise Fail("%s: item %d has no name" % (path, it["id"]))
    return items


def validate(items, origin):
    seen = set()
    for it in items:
        if it["id"] in seen:
            raise Fail("%s: duplicate item id %d" % (origin, it["id"]))
        seen.add(it["id"])


def emit(items, out_path):
    parts = ['#include "item.h"', "", "namespace l3d {", "namespace {", ""]
    for it in items:
        parts.append('static const char n_%d[] = "%s";' % (it["id"], esc(it["name"])))
    v_all = []
    for it in items:
        v = ["%d" % x for x in it["vocab"]]
        while len(v) < MAX_VOCAB:
            v.append("0")
        v_all.append(v)
    parts.append("static const ItemDef defs[] = {")
    for it, v in zip(items, v_all):
        parts.append("    {%d, %d, %s, %d, %d, n_%d, {%s}, %d}," % (
            it["id"], it["cat"], "true" if it["stack"] else "false",
            it["max"], it["weight"], it["id"], ", ".join(v), len(it["vocab"])))
    parts.append("};")
    parts.append("")
    parts.append("} // namespace")
    parts.append("")
    parts.append("const ItemBank content_items() {")
    parts.append("    ItemBank b{defs, sizeof(defs) / sizeof(defs[0])};")
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
            files += sorted(p.glob("*.item"))
        elif p.is_file():
            files.append(p)
        else:
            raise Fail("not found: %s" % raw)
    if not files:
        raise Fail("no .item inputs")
    return files


def main(argv):
    check = "--check" in argv
    argv = [a for a in argv if a != "--check"]
    try:
        if check:
            if not argv:
                print("usage: build_items.py --check <item-or-dir>...",
                      file=sys.stderr)
                return 2
            all_items = []
            for f in collect(argv):
                all_items += parse_item_file(f)
            validate(all_items, "content")
            print("item content OK: %d item(s)" % len(all_items))
            return 0
        if len(argv) < 2:
            print("usage: build_items.py <item-or-dir>... <output.cpp>",
                  file=sys.stderr)
            return 2
        *ins, out = argv
        all_items = []
        for f in collect(ins):
            all_items += parse_item_file(f)
        validate(all_items, "content")
        emit(all_items, Path(out))
        print("generated %s: %d item(s)" % (out, len(all_items)))
        return 0
    except Fail as ex:
        print("build_items.py: error: %s" % ex, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
