"""Generate the German audio for one deck and write audio/manifest.json.

Provider: the one named by "final" in tools/voices.json (azure or elevenlabs, billed).
--provider edge makes free, temporary audio instead, for trying cards out.

  python3 tools/make_audio.py content/gefuehle             # shows what would be generated and billed
  python3 tools/make_audio.py content/gefuehle --paid      # actually generate it
  python3 tools/make_audio.py content/gefuehle --provider edge   # temporary audio, never committed
  python3 tools/make_audio.py content/gefuehle --only stolz,sauer
  python3 tools/make_audio.py content/gefuehle --dry-run   # list what would be generated
  python3 tools/make_audio.py content/gefuehle --force     # regenerate everything

Paid generation never happens as a side effect: without --paid, a billed provider
stops after printing how many files and characters would be billed.
Only files whose text, provider, model, voice or file format changed are generated again.
Spoken text comes from deckfmt.spoken_lines, the same function the checker uses.
Azure needs SPEECH_KEY and SPEECH_REGION, ElevenLabs needs ELEVENLABS_API_KEY; voices
are configured in tools/voices.json.

Temporary (edge-tts) audio must never be committed. While a deck's audio folder holds
any, this script keeps a .gitignore in it that hides the whole folder from git, and
removes that file again once every clip is final.
"""
import argparse, asyncio, json, os, sys, time, urllib.error, urllib.request
from xml.sax.saxutils import escape
from pathlib import Path
from deckfmt import ROOT, load_json, spoken_lines, strip


def voice_for(cfg, card_index, n):
    """Words: one voice. Examples: alternate between the example voices, card by card."""
    if n == 2:
        voices = cfg["example_voices"]
        return voices[card_index % len(voices)], cfg["speed_example"]
    return cfg["word_voice"], cfg["speed_word"]


def dialogue_turns(card):
    """The turns of an example written as a dialogue (lines starting with a dash), or None."""
    lines = [l.strip() for l in strip(card["example"]["de"]).split("\n")]
    if len(lines) < 2:
        return None
    return [l.lstrip("–—- ").strip() for l in lines]


def tts_elevenlabs(cfg, text, voice, speed, out):
    text, voice = text.replace("\n", " "), voice.split("|")[0]  # one voice only
    key = os.environ.get("ELEVENLABS_API_KEY")
    if not key:
        sys.exit("ELEVENLABS_API_KEY is not set")
    body = {"text": text, "model_id": cfg["model"],
            "voice_settings": {"stability": cfg.get("stability", 0.5),
                               "similarity_boost": cfg.get("similarity_boost", 0.75),
                               "speed": speed}}
    if cfg.get("language_code"):
        body["language_code"] = cfg["language_code"]
    req = urllib.request.Request(
        f"https://api.elevenlabs.io/v1/text-to-speech/{voice}?output_format={cfg.get('output_format', 'mp3_44100_128')}",
        data=json.dumps(body).encode(), method="POST",
        headers={"xi-api-key": key, "Content-Type": "application/json", "Accept": "audio/mpeg"})
    try:
        with urllib.request.urlopen(req, timeout=90) as r:
            out.write_bytes(r.read())
    except urllib.error.HTTPError as e:
        sys.exit(f"ElevenLabs {e.code}: {e.read().decode(errors='replace')[:300]}")


def tts_azure(cfg, text, voice, speed, out):
    """A dialogue (turns separated by "\n", voices by "|") is spoken by alternating voices."""
    key, region = os.environ.get("SPEECH_KEY"), os.environ.get("SPEECH_REGION")
    if not key or not region:
        sys.exit("SPEECH_KEY and SPEECH_REGION are not set")
    rate = f"{round((speed - 1) * 100):+d}%"
    voices = voice.split("|")
    turns = "".join(f"<voice name='{voices[i % len(voices)]}'><prosody rate='{rate}'>{escape(t)}</prosody>"
                    f"<break time='350ms'/></voice>" for i, t in enumerate(text.split("\n")))
    ssml = f"<speak version='1.0' xml:lang='{cfg['language']}'>{turns}</speak>"
    req = urllib.request.Request(
        f"https://{region}.tts.speech.microsoft.com/cognitiveservices/v1", data=ssml.encode(), method="POST",
        headers={"Ocp-Apim-Subscription-Key": key, "Content-Type": "application/ssml+xml",
                 "X-Microsoft-OutputFormat": cfg["output_format"], "User-Agent": "deutsch-mit-knoedel"})
    for attempt in range(4):  # 429: the free tier allows only a few requests per second
        try:
            with urllib.request.urlopen(req, timeout=90) as r:
                out.write_bytes(r.read())
            return
        except urllib.error.HTTPError as e:
            if e.code != 429 or attempt == 3:
                sys.exit(f"Azure {e.code}: {e.read().decode(errors='replace')[:300] or e.reason}")
            time.sleep(3 * (attempt + 1))


def tts_edge(cfg, text, voice, speed, out):
    import edge_tts  # pip3 install -U edge-tts
    text, voice = text.replace("\n", " "), voice.split("|")[0]  # one voice only
    rate = f"{round((speed - 1) * 100):+d}%"
    for attempt in range(5):  # the free service now and then answers 403; it works again a moment later
        try:
            asyncio.run(edge_tts.Communicate(text, voice, rate=rate).save(str(out)))
            return
        except Exception as e:
            if attempt == 4:
                sys.exit(f"edge-tts failed for {out.name}: {e}")
            time.sleep(2 * (attempt + 1))


PROVIDERS = {"azure": tts_azure, "elevenlabs": tts_elevenlabs, "edge": tts_edge}
PAID = {"azure", "elevenlabs"}
TEMPORARY = {"edge"}  # same set as in check_deck.py
IGNORE_NOTE = "# Temporary (edge-tts) audio is never committed. Written and removed by tools/make_audio.py.\n*\n"


def guard_temporary(audio, manifest):
    """Hide the folder from git while it holds temporary audio."""
    marker = audio / ".gitignore"
    if any(e.get("provider") in TEMPORARY for e in manifest["files"].values()):
        marker.write_text(IGNORE_NOTE, encoding="utf-8")
    elif marker.exists() and marker.read_text(encoding="utf-8") == IGNORE_NOTE:
        marker.unlink()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("deck_dir")
    ap.add_argument("--only", default="", help="comma-separated card ids")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--provider", choices=sorted(PROVIDERS), help='default: "final" in voices.json')
    ap.add_argument("--paid", action="store_true", help="allow calls to a paid provider")
    a = ap.parse_args()

    deck_dir = Path(a.deck_dir)
    deck = load_json(deck_dir / "deck.json")
    conf = load_json(ROOT / "tools" / "voices.json")
    provider = a.provider or conf["final"]
    cfg = conf[provider]
    synth = PROVIDERS[provider]
    paid = provider in PAID
    audio = deck_dir / "audio"
    if not a.dry_run:
        audio.mkdir(exist_ok=True)
    manifest_path = audio / "manifest.json"
    manifest = load_json(manifest_path) if manifest_path.exists() else {"format": 1, "files": {}}
    manifest["deck"], manifest["deck_version"] = deck["id"], deck["version"]
    only = set(filter(None, a.only.split(",")))
    plan, skipped = [], 0
    for k, card in enumerate(deck["cards"]):
        if only and card["id"] not in only:
            continue
        for n, text in spoken_lines(card).items():
            name = f"{card['id']}_{n}.mp3"
            voice, speed = voice_for(cfg, k, n)
            turns = dialogue_turns(card) if n == 2 else None
            if turns:
                # Question and answer in two voices; the manifest keeps the text as spoken_lines() has it.
                other = [v for v in cfg["example_voices"] if v != voice] or [voice]
                voice = f"{voice}|{other[0]}"
            entry = {"text": text, "provider": provider, "model": cfg.get("model"), "voice": voice, "speed": speed,
                     "output_format": cfg.get("output_format")}
            if not a.force and manifest["files"].get(name) == entry and (audio / name).exists():
                skipped += 1
            else:
                plan.append((name, text, voice, speed, entry, "\n".join(turns) if turns else text))

    if not only and not a.dry_run:  # clips of lines the deck no longer speaks
        wanted = {f"{c['id']}_{n}.mp3" for c in deck["cards"] for n in spoken_lines(c)}
        for name in sorted(set(manifest["files"]) - wanted):
            del manifest["files"][name]
            (audio / name).unlink(missing_ok=True)
            print(f"removed  {name}  (no longer in the deck)")
        if manifest_path.exists():
            manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
            guard_temporary(audio, manifest)

    chars = sum(len(p[1]) for p in plan)
    print(f"provider {provider}{' (paid)' if paid else ''}: {len(plan)} file(s), {chars} characters, {skipped} unchanged")
    if a.dry_run or not plan:
        for name, text, *_ in plan:
            print(f"  would make  {name:<22} {text}")
        return
    if paid and not a.paid:
        print("Nothing generated. This provider is billed: run again with --paid to generate.")
        return

    for name, text, voice, speed, entry, said in plan:
        synth(cfg, said, voice, speed, audio / name)
        manifest["files"][name] = entry
        manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        guard_temporary(audio, manifest)
        print(f"ok  {name:<22} {text}")
    print(f"\n{len(plan)} generated, {skipped} unchanged")


if __name__ == "__main__":
    main()
