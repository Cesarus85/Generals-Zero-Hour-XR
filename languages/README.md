# Language packs

A language pack is one plain-text file: `languages/<language>/generals.str`.

That is the whole format. UTF-8, one entry per label, editable in any editor, and
reviewable as a normal diff in a pull request. Translating the game means editing
strings in this directory and opening a PR; there is nothing to compile and no
binary to produce.

```
GUI:GameOptions
"ОПЦИИ ИГРЫ"
END
```

A label line, the translated string in double quotes, then `END`. Lines starting
with `//` are comments. Escapes are `\n`, `\t`, `\"` and `\\`. Leave the label
lines exactly as they are — the game looks strings up by label, so a changed
label is a string the game can no longer find.

## Why a .str and not a .csf

The engine has always read both: a compiled binary `.csf`, and this plain-text
`.str`, which was the development format. `GameTextManager::init()` prefers the
`.str` when one exists.

Until now the text format could only hold Latin-1 — every byte became the code
point with the same value — so a Russian or Greek translation could not be
written in it at all, and had to be shipped as a compiled `.csf` instead. That is
why every community translation is a `.big` archive that overwrites
`Data\English`: not because anyone wanted it that way, but because the reviewable
format could not carry the alphabet. `.str` files are now decoded as UTF-8
(`GameTextManager::translateCopy`), and the path the engine looks in is
per-language (`data/<language>/generals.str`), so any language can be a text file
again.

## Where it goes on a device

The engine asks for `data/<language>/generals.str` inside the game folder, with
the language being what the launcher's language setting maps to — `russian`,
`german`, `spanish`, `french`, `korean`, `polish`, `brazilian`, `chinese`. Copy
the file there and set the launcher's language; nothing else is needed.

The launcher's Diagnostics section will fetch these packs directly in a later
version. The format is settled first so that translations started now stay
valid.

## Converting an existing translation

A `.csf` cannot be renamed into a `.str` -- it is binary. Its text is UTF-16LE with
every byte bitwise inverted, wrapped in a table of contents. `scripts/language/csf2str.py`
undoes that and writes the UTF-8 text file:

```
python3 scripts/language/csf2str.py Generals.csf -o languages/<language>/generals.str
```

It also reads a `.big` archive directly, since community translations ship as one,
and takes the first `generals.csf` inside whatever language folder the archive
happens to file it under:

```
python3 scripts/language/csf2str.py 00RussianZH.big -o languages/russian/generals.str
```

Nothing else is needed: the output is the pack.

## Russian

`languages/russian/generals.str` was produced from the community `00RussianZH.big`
translation, whose `Data\English\generals.csf` was decompiled back to text: 3991
labels, 3602 of them carrying Cyrillic. It is now maintained here as source
rather than as a binary nobody can read.

One inherited quirk is preserved deliberately: the pack contains a label
`GUI:CÀontrolBarBack` with a stray byte in it. The game looks up
`GUI:ControlBarBack`, so that entry never applied in the original pack either.
It is kept verbatim rather than silently "corrected", because correcting it is a
translation change and belongs in a pull request that says so.
