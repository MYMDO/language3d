# Quest content format (.quest)
#
# Strict line-based format compiled by tools/build_quests.py into static C++
# tables. The build FAILS on any validation error (duplicate ids, unknown
# objective/condition types, dangling references, over-long texts) —
# invalid content breaks CI by construction.
#
#   quest <u16 id>
#   title <1..128 chars>
#   description <1..256 chars>
#   prereq <questId> <ACTIVE|COMPLETED|CLAIMED>      # up to 2, may omit
#   reward <XP:n|ITEM:i:c|FLAG:b|COUNTER:i:n>        # up to 4, may omit
#   objective <TALK|REACH|COLLECT|GIVE|USE|INSPECT>
#             [tag=N] [item=N] [count=N] [npc=N]
#             [if_item=I:C] [if_flag=B] [if_counter=I:T] [if_level=L]
#                                                   # up to 8, ≥1 required
#
# Objective parameters:
#   TALK tag=<npc_tag> | REACH tag=<location_tag> | INSPECT tag=<target>
#   COLLECT item=<id> count=<n> | GIVE item=<id> count=<n> npc=<recipient>
#   USE item=<id> [tag=<location context, 0 = anywhere>]
# Conditions (optional, at most one per objective):
#   if_item=<id>:<count> = player holds | if_flag=<0..31>
#   if_counter=<idx>:<threshold> | if_level=<n>
# Tags reference the shared content namespaces (NPC tags, Region3 tags,
# item ids). `#` starts a comment; blank lines ignored.
