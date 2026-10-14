"""Master inventory of every word and expression already taught. Prevents duplicates.

  python3 tools/inventory.py report                      # totals, duplicates
  python3 tools/inventory.py check content/koerper       # before writing: which headwords are taken?
  python3 tools/inventory.py add content/koerper         # after the deck is final: record its cards

A lemma is a card exactly once in the whole curriculum (role "card"). It may appear in
example sentences anywhere. Rows with role "expression" were taught inside sentences
or videos; reusing them as a card is allowed but reported.
"""
import csv, sys
from pathlib import Path
from deckfmt import ROOT, strip, load_json

INV = ROOT / "curriculum" / "inventory.csv"
FIELDS = ["lemma", "pos", "article", "level_est", "topic", "role", "deck", "status", "notes"]


def key(lemma, pos):
    """'sich anziehen' and 'anziehen' are the same card; case and 'sich' do not matter."""
    words = lemma.lower().replace("?", "").split()
    if words and words[0] == "sich":
        words = words[1:]
    return (" ".join(words), pos)


def load():
    with INV.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def card_lemma(card):
    return " ".join(x for x in (card.get("lead", ""), strip(card["headword"])) if x).strip()


def report():
    rows = load()
    print(f"{len(rows)} rows")
    by = {}
    for r in rows:
        by.setdefault((r["level_est"], r["role"]), 0)
        by[(r["level_est"], r["role"])] += 1
    for (lvl, role), n in sorted(by.items()):
        print(f"  {lvl:<3} {role:<10} {n}")
    seen = {}
    for r in rows:
        if r["role"] == "card":
            seen.setdefault(key(r["lemma"], r["pos"]), []).append(r["deck"])
    dups = {k: v for k, v in seen.items() if len(v) > 1}
    print(f"\nduplicate cards: {len(dups)}")
    for (lemma, pos), decks in sorted(dups.items()):
        print(f"  {lemma} ({pos}): {', '.join(decks)}")


def check(deck_dir, quiet=False):
    deck = load_json(Path(deck_dir) / "deck.json")
    rows = load()
    taken = {key(r["lemma"], r["pos"]): r for r in rows if r["deck"] != deck["id"]}
    last = {}
    for r in rows:
        if r["deck"] != deck["id"] and r["pos"] == "verb":
            last.setdefault(r["lemma"].lower().split()[-1], []).append(r)
    problems = 0
    for c in deck["cards"]:
        lemma = card_lemma(c)
        hit = taken.get(key(lemma, c["pos"]))
        if hit and hit["role"] == "card":
            print(f"DUPLICATE  {lemma}: already a card in '{hit['deck']}' ({hit['status']})")
            problems += 1
        elif hit:
            print(f"note       {lemma}: was taught as an expression in '{hit['deck']}'")
        if c["pos"] == "verb":
            for r in last.get(lemma.lower().split()[-1], []):
                if key(r["lemma"], "verb") != key(lemma, "verb"):
                    print(f"related    {lemma} ~ {r['lemma']} ('{r['deck']}')")
    if not quiet:
        print(f"\n{len(deck['cards'])} cards checked, {problems} duplicate(s)")
    return problems


def add(deck_dir):
    deck = load_json(Path(deck_dir) / "deck.json")
    if check(deck_dir, quiet=True):
        sys.exit("not added: fix duplicates first")
    rows = [r for r in load() if r["deck"] != deck["id"]]  # re-adding a deck replaces its rows
    for c in deck["cards"]:
        rows.append({"lemma": card_lemma(c), "pos": c["pos"],
                     "article": c.get("noun", {}).get("article", ""), "level_est": deck["level"].split("-")[0],
                     "topic": c.get("topic", ""), "role": "card", "deck": deck["id"],
                     "status": deck.get("review", {}).get("status", "draft"), "notes": ""})
    with INV.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(rows)
    print(f"recorded {len(deck['cards'])} cards of '{deck['id']}'")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "report"
    if cmd == "report":
        report()
    elif cmd in ("check", "add") and len(sys.argv) > 2:
        sys.exit(1 if (check(sys.argv[2]) if cmd == "check" else add(sys.argv[2])) else 0)
    else:
        sys.exit(__doc__)
