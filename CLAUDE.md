# CLAUDE.md

macgip: a macOS driver for wired gamepads that speak GIP, compatible with Xbox controllers. Read
`README.md` first, then `notes/architecture.md` for the current plan and `notes/protocol.md` for
the verified packet formats.

## Project rules

- Do not change system security settings (SIP, boot arguments, system extension developer mode).
  Stop and explain if a step needs them.
- Ask before installing anything or running `sudo`.
- Write only the packets listed in `notes/protocol.md` to the controller. No firmware commands,
  no fuzzing of the OUT endpoint.
- Keep licences clean. This project is MIT. xpad and xone are GPL-2.0: read them for facts, never
  copy code. SDL is zlib: copy from SDL if you must copy.
- Record every finding in `notes/`. Raw logs go in `notes/captures/` as `.txt`.
- Trademarks: say "for Xbox" or "compatible with Xbox", never use a trademark as the name of this
  project or its parts. Keep the trademark notice in `README.md` prominent. Quoted device
  strings, identifiers, and log output stay verbatim.

## Language

Always write in ASD-STE100 Simplified Technical English, in plain language. This
applies to chat replies, docstrings and code comments, commit messages and PR
text, and generated technical content (READMEs, docs, ADRs, changelogs, release
notes, error messages, CLI help).

- One instruction per sentence. Keep sentences short (max 20 words; max 25 in
  descriptive text).
- Use the active voice and the present tense.
- Use one approved word for one meaning. Do not use a synonym for variety.
- Write a command as an imperative: "Run the migration." Not "The migration
  should be run."
- Do not use a noun cluster of more than 3 words.
- Do not drop an article ("the", "a"). Do not use slang, an idiom, or humor.
- Keep identifiers, API names, and quoted output verbatim.

No AI speak:

- No em-dashes. Use a period, a comma, a colon, or parentheses.
- Do not say "load bearing" or "seam". Name what actually calls the code and
  what breaks without it.
- No filler openers ("Great question", "Certainly", "Let's dive in").
- No inflated words: leverage, delve, elevate, unlock, harness, robust,
  seamless, comprehensive, cutting-edge, tapestry, landscape, journey.
- No "not just X, but Y". No rhetorical question as an opener. No closing
  paragraph that repeats the answer.
- No bold for emphasis inside a sentence. Bold a label only.
- Say "use", not "utilize". Say "so", not "thus". Say "before", not "prior to".

## Commits

Short, imperative, capitalized subjects. No Conventional Commits prefix. Body only when the
"why" is not obvious. No agent attribution in commits or pull requests.
