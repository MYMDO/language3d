# Item content format (.item)
#
# Strict line-based format compiled by tools/build_items.py into static C++
# tables. The build FAILS on any validation error (duplicate ids, unknown
# category, bad stacking rules, over-long names) — invalid content breaks
# CI by construction.
#
#   item <u16 id> category=<food|tool|document|key|ticket|book|clothing|
#                        currency|artifact|quest>
#                stackable=<0|1> max=<u16, >=1> weight=<u16>
#                [vocab=<u16,… up to 4>]
#   name <1..64 chars, single line>
#
# Rules:
# - Non-stackable items must declare max=1 (one instance per slot).
# - `vocab` ids reference the SHARED content vocabulary namespace also used
#   by dialogue nodes (e.g. 101=greeting, 103=station). Items duplicate no
#   language data; the future vocabulary subsystem resolves ids to entries.
# - `#` starts a comment; blank lines ignored.
