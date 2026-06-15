# Pokémon Emerald: Nuzlocke Edition

A Pokémon Emerald ROM hack that enforces a complete Nuzlocke ruleset **inside the game engine**. Instead of relying on player self-discipline, the game itself makes permanent death permanent: fallen Pokémon are sent to a dedicated Graveyard, revival is removed, captures are limited to one per area, and a lost run becomes an unrecoverable memorial save.

## Overview

Most Nuzlocke runs are honor-system challenges. This hack removes the loopholes. Every mechanic a player could normally use to escape a death — reviving, healing a fainted Pokémon, soft-resetting before the save, breeding replacements, trading a corpse away — is closed off in code. The rules activate automatically once the rival hands over your Poké Balls, and from that point the game tracks deaths, catch areas, and the run-loss state in save data.

The goal is a faithful, self-enforcing Nuzlocke that still plays like normal Emerald in every other respect. By design, the hack makes **no changes to Pokémon species, stats, movesets, type charts, trainers, maps, or story** — only the rules that govern survival and capture. It is built on the [pret/pokeemerald](https://github.com/pret/pokeemerald) decompilation via the *Pokémon Emerald Revamped* base (see [Technical Notes](#technical-notes)).

## Features

### Nuzlocke Rules

#### Forced Nicknames

Every Pokémon you obtain must be named before it joins you. The "Would you like to give it a nickname?" Yes/No prompt is skipped and the naming screen opens automatically for wild captures, the starter, the Johto starters, gift Pokémon (Beldum, Castform), fossil revivals (Lileep, Anorith), and hatched eggs. Submitting a blank or spaces-only name is rejected.

Impact: Guarantees every member of your run is a named, deliberate choice, reinforcing the personal stakes of a Nuzlocke.

#### Permanent Death

When a Pokémon hits 0 HP in a battle that counts, it dies permanently the moment the battle ends. It is removed from the party (the party is then compacted) and can never battle, be healed, be revived, be traded, be bred, or be deposited in Day Care again.

Impact: A faint is final. There is no "it only fainted" — losing a Pokémon means losing it for good.

#### Graveyard Storage

Dead Pokémon are not deleted; they are moved to the last two PC boxes, reserved as the Graveyard (capacity 60, filled in death order). These boxes are reached only through a separate **Graveyard's PC** menu entry and shown as "Graveyard 1" and "Graveyard 2". The Graveyard is read-only: you cannot withdraw, deposit, move, reorder, rename, or re-wallpaper anything in it. Living storage uses boxes 1–12, and caught Pokémon are never placed in a Graveyard box.

Impact: Your fallen team is preserved as a permanent memorial you can revisit, fully separated from your living Pokémon.

#### Held-Item Recovery on Death

If a Pokémon dies holding an item, the game first tries to move that item to your Bag. If the Bag is full, the item stays on the dead Pokémon and can be retrieved later from the Graveyard's PC. The item is never destroyed, and recovering it never restores the Pokémon. If your Bag is also full at the moment you try to retrieve an item from the Graveyard, you'll see a "Bag is full" message, the item stays with the Pokémon, and you can try again after making room.

Impact: You don't permanently lose valuable held items to death, but you also can't exploit item recovery to bring a Pokémon back.

#### First Encounter Per Catch Area

Once the challenge begins, only the **first valid wild encounter** in each catch area may be caught. Routes, caves, water routes, underwater maps, and the Safari Zone are each one area (multi-floor dungeons count as a single shared area). Any finished wild battle — caught, fainted, fled, or run from — consumes that area. After that, throwing a Poké Ball there is blocked with a message, and the ball is not consumed. Catch-area state persists across save/load.

Impact: The classic "one catch per area" rule is enforced for you; you can't accidentally (or deliberately) catch a second Pokémon in an area.

#### Legendary Capture Ban

Legendary Pokémon cannot be caught. Poké Balls and Safari Balls are blocked against them (Master Ball included), roaming Latios/Latias are blocked, and legendary encounters do not count as your area's encounter or consume the area. The ban uses both the battle's legendary/roamer flags and a species backstop list.

Impact: You can still battle legendaries, but they can never become an area's encounter — the challenge stays focused on standard Pokémon.

#### Day Care & Breeding Disabled

The Day Care refuses all deposits with a custom "hard mode" message, and egg production is turned off entirely. Game-provided gift/event eggs still work and still hatch, with mandatory nicknaming on hatch. (Pokémon already deposited in a pre-hack save can still be retrieved, so none are stranded.)

Impact: No passive leveling and no bred replacements — every Pokémon must be earned in the field.

#### Trade Restrictions

Player-to-player trading is disabled (the Cable Club Trade Center refuses with an in-character message) to stop players from trading a dead Pokémon away or importing a fresh one. In-game NPC trades remain available; their Pokémon arrive already nicknamed (as in the base game) and otherwise behave like any other living Pokémon.

Impact: You can't launder deaths through the link cable, but scripted story trades still work.

### Battle Changes

#### Battle Validity Classification

Deaths only count in trainer battles, wild battles, and the six Battle Frontier facilities that use your own Pokémon. Link battles, the Safari Zone (where you never send out your own Pokémon, so nothing can die — though Safari battles still consume catch areas), recorded battles, Wally's tutorial catch, and Birch's first-battle tutorial are all excluded from counting as deaths.

Impact: Pokémon only die where a Nuzlocke says they should — friendly and tutorial battles are safe.

#### Link Battles Stay Friendly

Link battles remain fully playable, and a Pokémon that faints in one is never counted as dead.

Impact: You can compete with friends without risking your run.

#### Battle Frontier Handling

In the Battle Tower, Dome, Palace, Arena, Pike, and Pyramid — facilities that fight with your real Pokémon — faints are recorded during the challenge and applied to the Graveyard after the run ends and your full party is restored (and before it is healed), matched by identity so per-round reordering is handled. The Battle Factory is excluded because it uses rental Pokémon, so its faints never count.

Impact: The Frontier carries real stakes where it uses your team, but rental-based facilities don't endanger it.

#### Safari Zone Ball Blocking

In the Safari Zone, the first-encounter and legendary rules apply to the Safari Ball throw action itself, with dedicated in-battle messages. A blocked throw costs the turn but does not consume a Safari Ball.

Impact: Catch-area limits are enforced consistently even in the Safari Zone's unique battle flow.

### Pokémon Changes

By design, the hack does **not** alter species, base stats, abilities, movesets, learnsets, evolutions, or the type chart (all explicitly out of scope). The only change to a Pokémon's standing is the permanent-death system above and the healing/revival restrictions below.

### Item Changes

#### Revive & Max Revive Removed

Revive and Max Revive are pulled from the game's reachable economy. Shop inventories no longer list them, hidden-item and field-pickup Revives become Super Potions, and field-pickup Max Revives become Max Potions.

Impact: There is no supply of revival items to tempt or enable bringing a Pokémon back.

#### Revival Effects Neutralized

Even if a Revive-flagged item is somehow used, it performs no HP restoration and cannot bring a fainted Pokémon back. Sacred Ash is disabled (it can no longer be used from the menu).

Impact: Closes every item path that could revive a dead Pokémon, including edge cases.

#### Fainted Pokémon Cannot Be Healed

Normal healing items never restore a Pokémon that is at 0 HP. Combined with permanent death (which removes the fainted Pokémon from the party entirely), there is no way to heal a Pokémon back into usability.

Impact: Healing only ever applies to living Pokémon — a corpse stays a corpse.

### Overworld Changes

#### Nuzlocke Activation & Announcement

The challenge officially begins the moment the rival gives you your Poké Balls in Professor Birch's Lab. A fanfare and on-screen announcement mark the start, and only from this point do encounters begin consuming catch areas.

Impact: Gives a clear, in-world starting line; nothing you do beforehand counts against you.

#### Whiteout Auto-Recovery

If your whole party is wiped but living Pokémon remain in boxes 1–12, the game automatically withdraws the first available living, non-Egg Pokémon into your party during the whiteout, before the standard heal. Only Pokémon that fall in a counting battle die; a party knocked out by field damage such as poison is simply healed as usual and never ends your run.

Impact: You never resume play with an empty or egg-only party as long as a living Pokémon exists in storage.

#### Run Loss & Memorial Save

If no living, non-Egg Pokémon remain anywhere (party and living boxes 1–12; eggs and Graveyard Pokémon never count), the run is lost. A "GAME OVER" message is shown, a memorial save is written (never deleting or corrupting your file), and the game returns to the title screen. Loading a memorial save immediately re-triggers the Game Over, so a lost run can never resume normal play.

Impact: A wipe with nothing left is a true end-state — the run is over and the save stands as a record of it.

#### Anti-Reset Auto-Save

After any battle in which a Pokémon dies, the game automatically performs a full save at the first safe overworld moment (before any new battle can start), writing the death, party compaction, item transfer, and Graveyard to the save.

Impact: You can't soft-reset to undo a death — by the time you regain control, it's already saved.

### Quality of Life

#### Faster Default Text Speed

New games default to **Fast** text speed instead of Mid.

Impact: Less waiting on text out of the box; still adjustable in Options.

### UI Improvements

#### Graveyard's PC Interface

The PC menu gains a "Graveyard's PC" option directly below "Someone's PC". It opens straight into the Graveyard boxes in a movement-locked, read-only view; the two boxes display as "Graveyard 1" and "Graveyard 2"; and the only available action on a dead Pokémon is sending its held item to the Bag.

Impact: A clean, dedicated memorial viewer that can't be used to disturb the dead.

#### PC Wallpaper Scheme

Living boxes 1–12 use a repeating Forest / City / Savanna / Desert wallpaper rotation by default (you can still change them freely afterward). The "Simple" wallpaper is reserved exclusively for the Graveyard boxes, is removed from the normal wallpaper-selection menu, and Graveyard boxes can never have their wallpaper changed.

Impact: The Graveyard is instantly recognizable, and its appearance is locked.

#### Game Over Title Window

The run-loss screen draws a centered "GAME OVER" title window above the memorial message.

Impact: A clear, deliberate presentation of the end of a run.

### Technical Features

#### No Save-Format Changes

"Death" is represented purely by a Pokémon's presence in a Graveyard box — no new save data is added. Catch-area and bookkeeping state reuse existing unused flags. Existing/pre-hack saves load normally; any living Pokémon found in the Graveyard boxes of an old save are relocated to boxes 1–12 once, so none are ever stranded or turned into memorials.

Impact: Stable saves and backward compatibility, with the entire ruleset layered on top of the vanilla save structure.

## Vanilla Differences

A concise summary of everything that differs from pret/pokeemerald in the Nuzlocke layer:

- Nicknaming is mandatory and automatic for all obtained Pokémon; blank names are rejected.
- Fainting in a counting battle is permanent death; the Pokémon is removed from the party.
- The last two PC boxes are a read-only Graveyard, accessed via a new "Graveyard's PC" menu entry, shown as "Graveyard 1/2".
- Living storage is limited to boxes 1–12 for both navigation and capture.
- Held items are pulled to the Bag on death (or kept in the Graveyard for later retrieval).
- One catch per catch area after the challenge starts; consumed areas block Poké Balls (ball not consumed).
- Legendary Pokémon cannot be caught (all ball types, including Master Ball and Safari Ball).
- The challenge activates when the rival gives Poké Balls, with an on-screen announcement.
- Day Care deposits and egg breeding are disabled; gift/event eggs still hatch (and force nicknames).
- Player-to-player trading is disabled; NPC trades and link battles remain.
- Link-battle and Battle Factory faints never count; the six own-Pokémon Frontier facilities do.
- Revive/Max Revive removed from shops, hidden items, and field pickups (replaced with Super/Max Potion); revival effects and Sacred Ash are neutralized; fainted Pokémon can't be healed.
- Whiteout auto-recovers a living boxed Pokémon; with none left anywhere, the run ends in a memorial Game Over save.
- A full auto-save fires after any death to prevent soft-reset abuse.
- PC wallpapers: living boxes rotate Forest/City/Savanna/Desert; "Simple" is Graveyard-only.
- New-game default text speed is Fast.
- No changes to Pokémon data, trainers, maps, encounters, or story.

## Technical Notes

- **Base repository:** [pret/pokeemerald](https://github.com/pret/pokeemerald). This project is layered on top of the *Pokémon Emerald Revamped* base (which itself modernizes engine, audio, and graphics tooling over pret); the Nuzlocke ruleset documented here is this hack's contribution on top of that base.
- **Design contract:** the authoritative, developer-facing implementation contract for the Nuzlocke systems lives in `docs/NuzlockeSpecification.md`. It carries its own document-revision number, which is independent of this hack's release version below. Contributors should read it before changing any Nuzlocke behavior.
- **Build requirements:** Build from source with the standard pokeemerald toolchain (a modern `arm-none-eabi` GCC via devkitARM works; follow the pret pokeemerald INSTALL guide for your OS). No ROM files are included or distributed. Build with `make -j$(nproc)` (e.g. `make -j4`). Because the hack intentionally changes gameplay, `make compare` is **not** expected to match vanilla Emerald.
- **Build fixes in this branch:** the Makefile includes `spritesheet_rules.mk`, and object-event graphics are sourced as pre-sliced `.4bpp` frames, to correctly build the Revamped base's overworld sprites. A `preproc` signedness fix is also included.
- **Emulator recommendations:** Any accurate GBA emulator (e.g. mGBA). Save behavior — including the auto-save-after-death and memorial-save flow — assumes correct flash save emulation, so use an emulator/core with reliable GBA save support.

## Known Limitations

- Optional Nuzlocke clauses are intentionally **not** enforced: there is no Dupes clause, Species clause, or Shiny clause. Captures are limited only by the first-encounter-per-area and legendary rules above.
- The Graveyard holds at most 60 Pokémon (two boxes of 30). If it is ever completely full, a newly fallen Pokémon is left in place rather than lost.
- Eggs never count as living Pokémon; a party/storage containing only eggs is treated as a lost run.
- Battle Frontier deaths are committed only when a challenge is completed through the facility's lobby. Resetting or quitting a challenge before it ends discards the faints it recorded — but the challenge itself is forfeited at the same time, so this is not a way to keep progress without consequences.
- Maps with no wild-encounter table are not catch areas, so scripted static encounters on such maps (e.g. Sudowoodo) remain catchable — a deliberate, documented exception.
- The legendary ban relies on battle legendary/roamer flags plus a fixed species list; any future non-flagged legendary encounter would need to be added to that list.
- Day Care *deposits* are disabled, but retrieval of Pokémon already deposited in a pre-hack save is intentionally left working.
- All player trading is disabled wholesale (the Trade Center is blocked), not just specific trades.
- `make compare` will not match vanilla Emerald on this branch.

## Changelog

### v1.0.0 — Nuzlocke Edition

- Implemented the full Nuzlocke ruleset: forced nicknames, permanent death, Graveyard storage, held-item recovery, first-encounter-per-area, legendary capture ban, whiteout recovery, run-loss/memorial save, anti-reset auto-save, Day Care/breeding disable, trade restrictions, revive removal, and Battle Frontier death handling (commit *Implement Pokémon Emerald Nuzlocke Edition*).
- Added new-game default text speed Fast and the PC wallpaper scheme (Graveyard "Simple", living-box rotation).
- Build/toolchain fixes for the Revamped overworld-sprite pipeline and a `preproc` signedness fix.

### Base

- Synced with the *Pokémon Emerald Revamped* base and merged upstream pret/pokeemerald history (engine, audio, and tooling updates) underneath the Nuzlocke layer.

## Credits

Built on the [pret Pokémon Emerald decompilation](https://github.com/pret/pokeemerald) via the Pokémon Emerald Revamped base. This repository contains source-code changes only and does not include any copyrighted ROM files.
