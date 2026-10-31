#!/usr/bin/env python3
"""Names are never translated or bent (owner, 2026-10-31): this core's own
name (ZEsarUX), the machine's (Next, Spectrum), its system's (NextZXOS) and
its programs' (the Browser, NextZXOS's file browser), and Chimera's, are
written as they are in every one of the 26 language tables. A language
whose grammar puts endings on names says the sentence with a common noun
beside the name instead ("pocitac Next", "Next-tietokoneen"). This checks:

- every name an English text holds is in that text in every language, as
  it is written;
- no letter is glued to either side of a name in any language but English
  (whose "Next's" is its own grammar): no case ending, no genitive "s", no
  ending after an apostrophe or a colon ("Next'in", "ZEsarUX:n"), and no
  ending after a hyphen
  ("NextZXOS-szel", "Next-ul"): a word of five letters or fewer after a
  hyphen is taken for an ending. A common noun of six letters or more after
  a hyphen ("Next-tietokone") is a compound and is not glued; nor is a
  particle in another script (Korean's, Japanese's, Chinese's), which
  leaves the name whole.

  waterbox/tests/check-names.py [<languages folder>]

Prints each one as language key: what is wrong: text, and exits 1 when there
is any. Chimera's own check (tools/check-proper-nouns.py) knows only
Chimera's names; this one is the core's."""
import json
import os
import re
import sys
import unicodedata

here = os.path.dirname(os.path.abspath(__file__))
folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "languages")

# longest first, so that "NextZXOS" is not read as "Next" with letters glued
NAMES = sorted(["ZEsarUX", "NextZXOS", "Next", "Spectrum", "Browser", "Chimera"], key=len, reverse=True)
# an ending after one of these: Next'in, ZEsarUX:n
APOSTROPHES = ("'", "\u2019", ":")
HYPHENS = ("-", "\u2010", "\u2011")


def bends(ch):
    """a letter of an alphabet (Latin, Cyrillic, Greek ...): one glued to a
    name is an ending. Han, kana and Hangul are other scripts whose
    particles stand beside a name without changing it."""
    if not ch or not ch.isalpha():
        return False
    name = unicodedata.name(ch, "")
    return not any(s in name for s in ("CJK", "HIRAGANA", "KATAKANA", "HANGUL"))


def texts(path):
    """a table's texts, a key's plural forms as one text"""
    with open(path, encoding="utf-8") as f:
        table = json.load(f)["texts"]
    return {k: " | ".join(v.values()) if isinstance(v, dict) else v for k, v in table.items()}


def file_names_out(text):
    """file names and identifiers are not names in a sentence: tbblue.mmc,
    .nex, tbblue_loader.rom (a full stop that ends a sentence stays)"""
    return re.sub(r"\S*\w[._/]\w\S*|(?<!\w)\.\w+", " ", text)


def found(text):
    """each name in the text with what stands before and after it:
    [(name, character before, text after)]; a longer name hides the
    shorter one inside it"""
    out, taken = [], [False] * len(text)
    for n in NAMES:
        for m in re.finditer(re.escape(n), text):
            if any(taken[m.start():m.end()]):
                continue
            for i in range(m.start(), m.end()):
                taken[i] = True
            before = text[m.start() - 1] if m.start() > 0 else ""
            out.append((n, before, text[m.end():m.end() + 24]))
    return out


def bent(before, after):
    if bends(before) or bends(after[:1]):
        return True
    # an ending after an apostrophe or a colon: Next'in, ZEsarUX:n
    if after[:1] in APOSTROPHES and bends(after[1:2]):
        return True
    # an ending after a hyphen: NextZXOS-szel, Next-ul; Next-tietokone is a
    # compound
    if after[:1] in HYPHENS:
        word = re.match(r"\w*", after[1:]).group(0)
        if word and bends(word[0]) and len(word) <= 5:
            return True
    return False


english = texts(os.path.join(folder, "en.json"))
names_in = {k: sorted({n for n, _, _ in found(file_names_out(v))}) for k, v in english.items()}
bad = 0
for name in sorted(os.listdir(folder)):
    if not name.endswith(".json") or name == "en.json":
        continue
    code = name[:-5]
    for key, text in sorted(texts(os.path.join(folder, name)).items()):
        seen = found(file_names_out(text))
        for n in names_in.get(key, []):
            if not any(f[0] == n for f in seen):
                print("%s %s: %s is not there as itself: %s" % (code, key, n, text))
                bad += 1
        for n, before, after in seen:
            if bent(before, after):
                print("%s %s: %s is bent [%s%s%s]: %s" % (code, key, n, before, n, after[:8], text))
                bad += 1
print("%d names translated or bent in %s" % (bad, os.path.normpath(folder)), file=sys.stderr)
sys.exit(1 if bad else 0)
