# Vocabulary content format (.vocab)
#
# Strict line-based format compiled by tools/build_vocab.py into static C++
# tables. The build FAILS on any validation error (duplicate ids, unknown
# language/POS/CEFR/category, over-long lemma) — invalid content breaks CI
# by construction.
#
#   vocab <u16 id> lang=<und|en|de|pl|es|fr> pos=<noun|verb|adj|adv|phrase|
#                                            preposition|pronoun|numeral>
#                cefr=<A1|A2|B1|B2|C1|C2> cat=<label>
#   lemma <1..48 chars, single line; multiword with spaces allowed>
#
# Rules:
# - Ids live in the SHARED content namespace referenced by dialogue nodes
#   (vocab=...), items (vocab=...) and quests (topic ids are separate).
# - `cat` is a content label (greeting/transport/direction/food/...);
#   the engine stores it opaquely and never branches on it.
# - `#` starts a comment; blank lines ignored.
