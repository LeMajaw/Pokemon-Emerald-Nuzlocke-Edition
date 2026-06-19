# Nuzlocke Specification

Version: 1.5.1

Note on versioning: the Version number above tracks revisions of this specification document only. It is independent of the hack's release version, which is recorded in the README changelog. A higher spec version does not imply a newer build.

Revision 1.2.1 clarifies Battle Frontier own-Pokémon facility death handling, Simple wallpaper wording, Poké Ball behavior in consumed catch areas, Poké Ball behavior against legendary Pokémon, and Rule 4 held-item message wording.

Revision 1.3.0 reconciles the specification with the shipped implementation. It formalizes the anti-reset auto-save (new Rule 17), records the investigated special battle modes (Rule 15), documents the catch-area table as fixed and append-only with its flag reservation (Rule 16), clarifies the legendary ban ball coverage and the static-encounter map exception (Rule 16), the Revive/Max Revive enforcement method (Rule 12), the Battle Frontier deferred death handling (Rule 11), Day Care retrieval of pre-existing deposits (Rule 6), the Graveyard-full behavior (Rule 3.1), the Game Over title window and whiteout scan order (Rule 5), the reserved save flags (Technical Requirements), and the single non-rule quality-of-life default (Implementation Notes).

Revision 1.3.1 is a documentation-audit pass. It adds no new rules; it makes existing behavior explicit for long-term maintenance: the PC box-count dependency behind the 13/14 numbering (Rule 3.1), Graveyard held-item retrieval when the Bag is full (Rule 4), the field-faint vs battle-death distinction in whiteout and run-loss (Rule 5), the runtime-only lifetime of recorded Battle Frontier faints (Rule 11), precise reserved-flag ranges with a post-merge re-verification warning (Technical Requirements), the default nature of the living-box wallpaper mapping (PC Box Backgrounds), a warning against repurposing the catch-rule activation flag (Rule 16), and it reframes the Investigation Phase section as historical.

Revision 1.4.0 documents three behaviors that are now verified in-game after a regression-fix pass. Two are restored features that had been lost during branch migration: the optional Dupes Clause (Rule 16) and indoor Running Shoes (Implementation Notes). The third is the Game Over presentation contract — the immediate-loss and memorial-save paths must share a single display flow and present the same screen on a clean background (Rule 5). The Dupes Clause is no longer listed as out of scope anywhere in this document.

Revision 1.5.0 extends the anti-reset auto-save to catch-area consumption: a wild encounter that newly uses up its catch area now persists that state at the next safe overworld step, so a failed or unlucky first encounter cannot be soft-reset away (Rule 16, Rule 17). It also records the Link Stone as the single-player replacement for trade evolutions now that trading is disabled (Rule 9).

Revision 1.5.1 clarifies Nuzlocke death handling for special battle contexts. Link battles, rental/temporary facility battles, Battle Tents, e-Reader trainer battles, Trainer Hill, and Secret Base battles do not count as deaths. Steven-style partner battles count only the player's own selected Pokemon slots. Deferred Battle Frontier deaths now also reset Momentum when applied.

`Implementation note:` bullets describe how a requirement is satisfied in the shipped build. They are part of the implementation contract and must not be silently broken; they do not relax or override the requirements above them.

> **Scope note:** this document covers only the Nuzlocke ruleset. The hack's
> **difficulty systems** — recommended levels, modern party-wide EXP, the
> over-cap EXP restriction, and Momentum — are documented separately in
> `DifficultySpecification.md`.

## Purpose

This document defines the Nuzlocke rules that must be implemented natively into this Pokémon Emerald ROM hack.

This document is the source of truth for the current implementation scope.

Anything described here is in scope.

Anything not described here is out of scope unless explicitly approved later.

---

# Out Of Scope (Future Work)

The following features are intentionally NOT part of this implementation:

* Shiny clause
* Species clause
* Additional trainer balancing
* Additional Pokémon balancing
* Story changes
* Region changes
* New Pokémon species
* New maps
* New towns
* New items
* Difficulty balancing unrelated to Nuzlocke rules

Do not implement any of the above.

---

# Core Design Philosophy

The player should no longer be able to bypass death through game mechanics.

A Pokémon that dies is gone forever.

The game itself should enforce the rules instead of relying on player self-discipline.

---

# Rule 1: Forced Nicknames

Every Pokémon obtained by the player must receive a nickname.

The player must not be able to skip nickname entry.

This applies to:

* Wild Pokémon captures
* NPC trade Pokémon
* Hatched Pokémon
* Gift Pokémon received from NPCs, scripts, and events (including the
  starter, Johto starters, fossil revivals, Beldum, and Castform)

The normal:

"Would you like to give a nickname?"

prompt should not be used.

The nickname flow should begin automatically.

`Implementation note:` the Yes/No prompt is skipped and the naming screen is
opened directly for wild captures (battle catch flow), the starter, the Johto
starters, fossil revivals (Lileep, Anorith), Beldum, Castform, and egg hatches.
The naming screen additionally rejects an empty or whitespace-only entry (for the
caught-mon and nickname naming templates that all forced-nickname paths use), so
the forced nickname cannot be bypassed by submitting nothing. In-game NPC trade
Pokémon arrive already nicknamed by the trade script (vanilla behavior) and
cannot be renamed by the receiver, which satisfies "follow nickname rules"
without a naming screen; those scripts are therefore left unmodified.

---

# Rule 2: Permanent Death

When a Pokémon reaches 0 HP in a valid battle, it becomes permanently dead.

A dead Pokémon must never become usable again.

Dead Pokémon may never:

* Battle
* Be healed back into usability
* Be revived
* Be traded
* Be deposited into Day Care
* Be selected for normal gameplay
* Be restored by any game mechanic

Death must persist in save data.

`Implementation note:` permanent death is applied once per battle, after the
battle is over and the party is final, before control returns to the overworld
(and therefore before any Pokémon Center or whiteout healing can touch a fainted
Pokémon). See Rule 15 for which battle types count and Rule 17 for persistence.

---

# Rule 3: Graveyard

Dead Pokémon should not simply disappear.

Dead Pokémon are moved into a dedicated Graveyard storage area, kept permanently separate from living Pokémon and exposed through a dedicated "Graveyard's PC" menu entry.

## 3.1 Storage Reservation

* Boxes 13 and 14 of the PC are reserved as Graveyard storage.
* Living Pokémon storage uses boxes 1 through 12 only.
* No new storage box is added and no save expansion is performed.
* Death order fills box 13 first, then box 14.
* Total Graveyard capacity is 60 Pokémon (two boxes of 30).

`Implementation note:` if the Graveyard is completely full (60 dead) when a
further Pokémon would die, that Pokémon is left in place rather than deleted or
lost. This is an extreme edge case but must never destroy data.

`Implementation note (box-count dependency — read before changing storage):`
the Graveyard is implemented as the *last two* PC boxes, and living storage as
*all boxes before them*, both derived from the total box count
(`TOTAL_BOXES_COUNT`). In the base game this count is 14, which is exactly why
the Graveyard is boxes 13 and 14 and living storage is boxes 1–12. The entire
scheme — the 13/14 numbering, the "1–12" living range, the 3.2 relocation, and
the run-loss scan — depends on this count remaining 14, which is consistent with
the out-of-scope rule against adding storage boxes or expanding the save. Do not
change the box count without revisiting this rule, Rule 3.2, the run-loss check
(Rule 5), and save compatibility; a change would silently move the Graveyard and
redefine which boxes count as living.

## 3.2 Existing Save Compatibility And Relocation

* Existing save files must continue to load.
* No new save data is introduced. "Dead" is represented solely by a Pokémon's presence in a Graveyard box (box 13 or 14).
* On load, if any living Pokémon are found in boxes 13 or 14, they are automatically relocated to the first available slot in boxes 1 through 12.
* A living Pokémon must never be converted into a memorial Pokémon.
* A living Pokémon must never be deleted or stranded.

`Implementation note:` the one-time relocation runs before any death is ever
written to the Graveyard, guarded by a single reused unused flag (see Technical
Requirements). If boxes 1–12 are somehow completely full, the Pokémon is left in
place rather than stranded or deleted.

## 3.3 Graveyard's PC

* The memorial system is exposed through a separate "Graveyard's PC" entry in the PC menu, distinct from "Someone's PC".
* Someone's PC must not provide normal access to the Graveyard boxes.
* Within Graveyard's PC, box 13 is shown as "Graveyard 1" and box 14 as "Graveyard 2".
* Only dead Pokémon appear in Graveyard's PC.
* Dead Pokémon are displayed in death order.

`Implementation note:` the "Graveyard's PC" entry sits directly below "Someone's
PC" in the PC menu. Someone's PC clamps navigation to boxes 1–12; Graveyard's PC
clamps navigation to boxes 13–14 and opens on "Graveyard 1".

## 3.4 Allowed And Forbidden Interactions

Graveyard's PC is read-only except for held-item retrieval.

Dead Pokémon may NOT be:

* Withdrawn
* Moved or reordered
* Deposited into (no Pokémon may be placed into a Graveyard box)
* Traded
* Given items
* Returned to the party or to living boxes

The only permitted interaction is taking a held item from a dead Pokémon.

Taking a held item:

* Must not restore the Pokémon.
* Must not move the Pokémon.
* Leaves the Pokémon permanently in the Graveyard.

`Implementation note:` Graveyard's PC runs in a movement-locked storage mode, so
withdrawing, depositing, moving, reordering, and giving items are all
unavailable. The only box-menu action offered on a dead Pokémon sends its held
item to the Bag without touching the Pokémon's species, slot, or death status.
Mail is not retrievable this way. Bag-full handling for this action is specified
in Rule 4.

## 3.5 Graveyard Background

* Graveyard boxes 13 and 14 always use the dedicated "Simple" background.
* The background of Graveyard boxes cannot be changed.
* Background changes are not permitted from Graveyard's PC.

`Implementation note:` see the PC Box Backgrounds section below for how the
Simple-only restriction is enforced (forced at read time; removed from the
living-box wallpaper-selection menu).

---

# PC Box Backgrounds

This section defines the background (wallpaper) scheme for the PC storage system.

## Someone's PC (Boxes 1-12)

Someone's PC uses only boxes 1 through 12.

At new game, the 12 accessible storage boxes are given four repeating background groups as their default wallpapers:

* Boxes 1, 5, 9: Forest
* Boxes 2, 6, 10: City
* Boxes 3, 7, 11: Savanna
* Boxes 4, 8, 12: Desert

This grouping is the new-game default only. Because players may freely re-wallpaper boxes 1–12 (see below), an individual living box's background may differ from this mapping later in a save; only the Graveyard restriction is permanent.

The City background must remain available in Someone's PC exactly as it is in the base game.

Players may continue changing backgrounds for normal storage boxes (1-12) through Someone's PC, as allowed by the base game.

## Simple Background (Graveyard Only)

The existing "Simple" background is reserved exclusively for the Graveyard system.

* The existing "Simple" background is reserved exclusively for Graveyard boxes 13 and 14.
* No new wallpaper art is required.
* Simple must not appear in Someone's PC wallpaper selection menu.
* Background changes must never affect Graveyard boxes 13 and 14.

`Implementation note:` the Graveyard boxes are forced to Simple at read time, so
this also holds for pre-hack saves regardless of any stored wallpaper value, and
Simple is removed from the living-box wallpaper-selection menu.

---

# Rule 4: Held Items On Death

If a Pokémon dies while holding an item:

* The game should immediately attempt to move the held item into the player's Bag.
* The item should be placed into the appropriate Bag pocket using normal item storage rules.

If the item is successfully moved to the Bag:

* If safely supported by the current flow, display a message indicating that the Pokémon's held item was placed in the Bag.

If the item cannot be moved because there is no available space:

* The item remains attached to the dead Pokémon in the Graveyard.
* If safely supported by the current flow, display a message indicating that the item remains with the Pokémon in the Graveyard because the Bag had no room.
* The player must be able to retrieve the item later from the Graveyard without restoring access to the Pokémon.

Do not require messages if adding them would complicate or destabilize the death flow.

Recovering the item must never allow recovering the Pokémon.

The Pokémon remains dead.

`Implementation note (retrieval with a full Bag):` taking a held item from the
Graveyard also handles a full Bag. If the item cannot be added, a "Bag is full"
message is shown, the item stays on the dead Pokémon, and nothing about the
Pokémon (species, slot, death status) changes. The player can attempt retrieval
again after making room in the Bag. This mirrors the death-time bag-full rule:
the item is never destroyed and the Pokémon is never restored or moved.

---

# Rule 5: Whiteout Handling

If the entire active party faints:

## Preferred Behavior

If living Pokémon exist in storage:

* The game should automatically recover using available living Pokémon.

`Implementation note:` recovery withdraws the first living, non-egg Pokémon found
by scanning living boxes in order — box 1 slot 1 through box 12 slot 30 — into the
party, before the standard whiteout heal. Graveyard boxes are never scanned and
eggs are never withdrawn.

## Fallback Behavior

If automatic recovery is unsafe:

* Use the safest alternative.

## Run Loss Condition

"No living Pokémon remain anywhere" means no living, non-egg Pokémon exists in the party or in living boxes 1 through 12. Graveyard boxes 13 and 14 never count as living.

If no living Pokémon remain anywhere:

* The run is considered lost.
* The game must not corrupt or delete the save file.
* Display a Game Over message.
* Return to the title screen.

The save becomes a memorial save:

* Loading a memorial save must immediately detect the run-loss state, display the Game Over message, and return to the title screen.
* Normal gameplay cannot continue from a run-loss save.

The implementation should safely detect this state.

`Implementation note:` the Game Over message is accompanied by a centred
"GAME OVER" title window drawn above the dialogue. The run-loss check runs both
at the end of a whiteout and on continuing a saved game, so a memorial save
re-triggers the Game Over on load and normal play can never resume from it.

`Implementation note (field faints are not deaths):` only battle deaths
(Rules 2 and 15) move Pokémon to the Graveyard. A party can also reach 0 HP
without any battle death — most commonly a field-poison whiteout — in which case
the fallen Pokémon are NOT dead: they remain in the party at 0 HP and are
restored by the normal whiteout heal, so auto-recovery does nothing and the run
is not lost. Consequently the run-loss check counts a party Pokémon as living by
its existence (a non-egg species is present), not by its current HP, because any
Pokémon that truly died has already been removed from the party. A future change
must preserve this distinction: testing party HP instead of party membership
would wrongly end runs on field faints.

## Game Over Presentation

The Game Over screen is reached by two paths: an immediate run loss (the active
party wipes with no living Pokémon left anywhere) and loading a memorial save.
Both paths must:

* use the same Game Over display flow and present the same Game Over screen, so
  the two are indistinguishable to the player;
* present a clean background, with no leftover overworld/map graphics visible
  behind the Game Over window;
* leave the memorial save non-playable - loading it re-triggers the Game Over
  and returns to the title screen, and normal play can never resume.

`Implementation note:` both paths reach the Game Over through the same field
callback, which clears the map background layers before the Game Over window is
drawn. Earlier builds entered the screen from different graphics states and left
stray overworld tiles around the window; the shared, background-clearing flow is
the fix and must be preserved. (The specific layers/registers cleared are an
implementation detail; the contract is "same flow, same clean screen, both
paths".)

---

# Rule 6: Day Care Disabled

Day Care must be disabled completely.

The player may not:

* Deposit Pokémon
* Train Pokémon through Day Care
* Breed Pokémon

The Day Care woman should say:

"Sorry, sweetheart, but you chose to play the game on hard mode. That means no special training for either you or your Pokémon."

Use normal Pokémon Emerald text formatting.

`Implementation note:` every Day Care deposit path refuses with the hard-mode
message, and egg production is disabled in the Day Care step routine. To honour
Rule 3.2 (never strand a living Pokémon), retrieval of a Pokémon that is already
deposited in a pre-hack save is intentionally left working; only new deposits and
breeding are blocked.

---

# Rule 7: Eggs

Eggs provided directly by the game are allowed.

Examples:

* Gift eggs
* Event eggs

When an egg hatches:

* Nickname entry must be mandatory.

Day Care breeding is not allowed.

---

# Rule 8: NPC Trades

NPC trades are allowed.

The received Pokémon must:

* Follow nickname rules
* Behave like any other living Pokémon

`Implementation note:` in-game NPC trade Pokémon arrive already nicknamed by the
trade script and behave like any other living Pokémon; no additional change is
required (see Rule 1).

---

# Rule 9: Player-To-Player Trades

Player-to-player Pokémon trading must be disabled.

Players may not exchange Pokémon.

This restriction exists to prevent bypassing death.

`Implementation note:` player-to-player trading is blocked at every normal link
entry point: the Cable Club Trade Center refuses with an in-character message,
wireless trade is removed from the Direct Corner service menus, and the Union
Room trading board refuses registration or trade offers. The link-battle
(Colosseum) service is unaffected.

`Implementation note (trade evolutions):` because trading is disabled, Pokémon
that normally evolve by trading instead evolve via the **Link Stone** item — used
from the party menu, consumed on use, and sold at the Lilycove Department Store
(3F). It reproduces both plain trade evolutions and held-item trade evolutions
(the latter still require the appropriate held item, exactly as a real trade
would). This keeps trade-only evolutions obtainable in single-player. See the
README "Item Changes" section.

---

# Rule 10: Link Battles

Link battles remain allowed.

Pokémon that faint during link battles:

* Do NOT count as dead.

Link battles are considered friendly competition.

---

# Rule 11: Battle Frontier Facilities

Battle Frontier facilities that use the player's own Pokémon count as real battles.

Pokémon that faint in the following facilities count as dead:

* Battle Tower
* Battle Dome
* Battle Palace
* Battle Arena
* Battle Pike
* Battle Pyramid

Battle Factory does NOT count because it uses rental Pokémon.

Pokémon that faint in Battle Factory are not sent to the Graveyard, and the player's real party is not affected.

Battle Tent formats do NOT count. They are side/facility challenge content, and
their temporary or restricted battle setup must not create permanent Nuzlocke
deaths or deferred Frontier death records.

`Implementation note:` because a frontier challenge heals and restores the full
party between rounds and at the end, deaths must not be applied mid-challenge.
Faints in the six own-Pokémon facilities are recorded during the challenge
(identified by personality value, robust to per-round reordering) and applied to
the Graveyard from the lobby script after the challenge ends — once the full
party is restored and before it is healed. The applied deaths are saved
synchronously at that point. Battle Factory and Battle Tent faints are never
recorded. Deferred deaths reset Momentum at the same point they are moved to the
Graveyard.

`Implementation note (persistence / reset window):` the recorded faints live
only in runtime memory until the end-of-challenge lobby script applies and saves
them; they are intentionally NOT covered by the Rule 17 anti-reset auto-save. If
the game is reset or the challenge is otherwise interrupted before that lobby
script runs, the recorded faints are discarded together with the interrupted
challenge. Committing frontier deaths only on normal challenge completion is the
accepted behavior; this is the one place a death is not soft-reset-proof, and it
is acceptable because an interrupted frontier challenge is itself forfeited.

---

# Rule 12: Revive And Max Revive

Revive and Max Revive are removed from normal gameplay.

## Obtainable Locations

If the player would normally receive:

* Revive

replace it with:

* Super Potion

If the player would normally receive:

* Max Revive

replace it with:

* Max Potion

Applies to:

* Field pickups
* Hidden items
* NPC gifts
* Direct item-giving scripts

## Everywhere Else

Revive and Max Revive should not be:

* Buyable
* Obtainable
* Visible
* Usable
* Offered by shops
* Offered by scripts
* Reachable through menus

If low-level item constants must remain for engine stability:

* Keep them internally only.
* Make them unreachable to the player.

`Implementation note:` enforcement in the shipped build is twofold:
(1) every reachable source is removed — Revive/Max Revive are deleted from all
Poké Mart inventories and every field pickup, hidden item, and item-ball script
that gave them now gives a Super Potion / Max Potion instead; and
(2) the revive effect itself is neutralized (see Rule 13), so a Revive-flagged
item performs no HP restoration even if one is somehow held (e.g. from a pre-hack
save). The `ITEM_REVIVE` / `ITEM_MAX_REVIVE` constants are retained internally
for engine stability and are inert. This satisfies "keep internally / make
unreachable"; the items are not separately hidden from the Bag menu because they
can no longer be obtained and have no effect.

---

# Rule 13: Revival Prevention

No game mechanic should revive a dead Pokémon.

Required:

* Revive cannot revive.
* Max Revive cannot revive.
* Sacred Ash cannot revive.
* Rare Candy cannot revive.

Any additional revival paths discovered during investigation should also be disabled.

`Implementation note:` in the item-effect routine, any Revive-flagged item clears
its revive effect and restores no HP. Normal healing items never restore a
fainted (0 HP) Pokémon. Sacred Ash is changed to a non-usable Bag item so it
cannot be applied to the party. Rare Candy is inherently safe — it never restores
HP, so it cannot make a fainted Pokémon usable — and required no change.

---

# Rule 14: Healing Restrictions

Normal healing should affect only living Pokémon.

Dead Pokémon must not return to usability through:

* Pokémon Centers
* Story healing
* Whiteout healing
* Full party heals
* PC operations
* Items
* Scripts
* Any global heal function

Dead means permanently dead.

`Implementation note:` because permanent death removes a fainted Pokémon from the
party into the Graveyard before any healing runs (Rule 2), all party-wide and
Pokémon Center heals only ever touch living Pokémon. Item healing additionally
no-ops on any 0 HP Pokémon (Rule 13).

---

# Rule 15: Battle Validity

Deaths only count in battle types intended by this specification.

Count as death:

* Trainer battles
* Wild battles
* Steven-style in-game partner battles, but only for the player's own selected
  party slots. Borrowed partner Pokémon do not count.
* Battle Frontier facilities that use the player's own Pokémon:

  * Battle Tower
  * Battle Dome
  * Battle Palace
  * Battle Arena
  * Battle Pike
  * Battle Pyramid

Do not count as death:

* Player link battles
* Battle Factory battles, because Battle Factory uses rental Pokémon
* Battle Tent battles
* e-Reader trainer battles
* Trainer Hill battles
* Secret Base battles

If additional special battle modes exist:

* Investigate them
* Document them
* Use the safest interpretation

## Investigated Special Battle Modes

The investigation required above has been completed. The following non-standard
battle types are explicitly excluded from counting as death, in addition to link
and Battle Factory battles:

* Safari Zone battles — the player does not battle with their own Pokémon, so no
  death can occur. (Safari battles still consume catch areas; see Rule 16.)
* Recorded battles — playback, not a live encounter.
* Battle Tent battles — side/facility challenge content with temporary or
  restricted setup.
* e-Reader trainer battles — special event/test battle flow that saves and
  restores the party.
* Trainer Hill battles — side/facility challenge content.
* Secret Base battles — fun/simulated/player-created content.
* Wally's tutorial catch on Route 102 — a scripted capture before the challenge
  begins.
* Professor Birch's first-battle tutorial — occurs before Poké Balls exist.

The six own-Pokémon Battle Frontier facilities are handled separately (Rule 11).
Battle Factory and Battle Tent faints are not recorded for deferred Frontier
death application.

---

# Rule 16 — First Encounter Per Catch Area

## Activation

* The first-encounter rule is inactive at the start of the game.
* It becomes active only after the rival gives the player Poké Balls.
* When it becomes active, the game should display a clear announcement that the Nuzlocke challenge has officially begun.
* Encounters that occur before Poké Balls are received do not count.

`Implementation note:` activation is gated on the existing flag set exactly when
the rival hands over the Poké Balls in Birch's Lab; no new flag is added. The
announcement is shown there with a fanfare. Contributors must not repurpose this
activation flag or set it anywhere else: catch-area tracking turns on the moment
the flag is set, so any additional setter would activate the first-encounter rule
early.

## Catch Area Concept

* Each catch area allows only one valid encounter.
* Catch areas include routes, caves, water routes, underwater maps, Safari areas, special maps, and any other map/location where wild Pokémon may be encountered.
* This specification defines behavior; the concrete catch-area list is part of the implementation.

`Implementation note:` each catch area is one map section that has a wild
encounter table. Multi-floor dungeons share a single section (one area); the
whole Safari Zone is one area; the Underwater 124/126 sections are separate from
their surface routes. There are 64 catch areas. The catch-area table is **fixed
and APPEND-ONLY**: each area's consumed state is stored in a reserved save flag
computed from the area's index in the table, so entries must never be removed,
reordered, or inserted, or existing saves would see the wrong areas as consumed
(see Technical Requirements for the reserved flag range).

## Valid Encounter Rule

* The first valid encounter in a catch area becomes that area's encounter.
* Once a valid encounter occurs, the area becomes consumed.

## Area Consumption

A catch area becomes consumed if the first valid encounter:

* is caught;
* faints;
* flees;
* the player runs away;
* the battle ends without a capture for any normal reason.

After an area is consumed:

* Poké Balls cannot be used to catch Pokémon in that catch area.
* The consumption is persisted by the Rule 17 anti-reset auto-save at the next
  safe overworld step, so the used-up area cannot be soft-reset away.

If the player attempts to use a Poké Ball in a consumed catch area:

* the action fails;
* the game displays a message explaining that the area's encounter has already been used;
* the Poké Ball is not consumed;
* the battle continues normally.

`Implementation note:` consumption is triggered by the end of any real wild
encounter (however it ended) on a catch-area map, once activation has occurred.
Trainer, link, recorded, legendary/roamer, Wally-tutorial, and first-battle
battles never consume an area. The ball block is enforced both for thrown Bag
balls and for the Safari Ball battle action; a blocked Safari throw costs the
turn but consumes no Safari Ball.

## Non-Consuming Sources

The following never consume a catch area:

* starter Pokémon;
* gift Pokémon;
* eggs;
* NPC trades;
* fossil revival Pokémon.

## Legendary Pokémon

Legendary Pokémon are completely banned from capture.

Legendary Pokémon:

* cannot be caught;
* do not count as valid encounters;
* do not consume a catch area;
* must be ignored by encounter tracking.

If the player attempts to use a Poké Ball on a legendary Pokémon:

* the action fails;
* the game displays a message explaining that legendary Pokémon cannot be caught in this challenge;
* the Poké Ball is not consumed;
* the battle continues normally.

If a legendary Pokémon appears before a valid encounter:

* the area remains unused;
* encounter tracking continues;
* the first non-legendary valid encounter becomes the area's encounter.

`Implementation note:` the block applies to **all** ball types, including the
Master Ball and the Safari Ball. A legendary is detected by the battle's
legendary/roamer flags (covering vanilla statics, the Regis, Kyogre/Groudon,
Rayquaza, the Southern Island Latis, and roamers) plus a species backstop list
as a safety net. Any future non-flagged legendary encounter path must be added to
that species list.

## Static Non-Legendary Encounters

Static non-legendary Pokémon follow normal encounter rules.

If a static non-legendary Pokémon is the first valid encounter in a catch area:

* it becomes the area's encounter;
* it may be caught;
* it consumes the area.

If the area has already been consumed:

* the encounter may still occur;
* Poké Balls remain blocked;
* the Pokémon cannot be caught.

Examples:

* Kecleon
* future Sudowoodo-style encounters
* future Voltorb/Electrode-style encounters
* any other non-legendary scripted encounter

`Implementation note:` catch-area logic is keyed on map sections that have a wild
encounter table. A static non-legendary on a map that is **not** in the
catch-area table (a map with no wild encounters) is therefore never tracked,
never consumes an area, and is never ball-blocked — it remains freely catchable.
This map-keyed exception was approved in the Rule 16 design review; statics on
maps that *are* catch areas behave exactly as described above.

## Optional: Dupes Clause

The Dupes Clause is an **optional** rule the player chooses once, at run start.
It is off by default (and off for every existing save), so the default behavior
is identical to the first-encounter rule above.

* When disabled (the default, and the state of every existing save), the clause
  has no effect: every first valid encounter counts and consumes its catch area
  exactly as in Rule 16, and duplicate species are fully catchable.
* The choice is offered exactly once, in Birch's Lab, immediately after the
  rival hands over the Poke Balls (before the challenge-start announcement).
  The default selection is NO, and a B press counts as NO.
* The choice is stored in `FLAG_NUZLOCKE_DUPES_CLAUSE` (a reused unused flag,
  `0x21`; no new save data). The player's own Trainer Card shows a small tag
  after the ID: `DA` (dupes allowed / clause off) or `DN` (dupes not allowed /
  clause on). The tag never appears on link partners' cards.
* When enabled, a wild encounter whose species the player already owns **alive**
  is ignored by first-encounter tracking: it cannot be caught (thrown Poke Balls
  and Safari Ball throws are blocked with a message, and no ball is consumed),
  and it does not consume the catch area, in any outcome.
* "Alive" means a non-egg member in the party (HP above zero) or anywhere in the
  living boxes (1-12). The Graveyard never counts (a species whose only members
  are dead may legally appear again) and eggs never count.
* A successful catch always consumes the area: a duplicate can never be caught,
  and checking after a catch would misread the just-caught Pokemon as its own
  duplicate. Ball-block priority is legendary > area-already-used > duplicate.

## Out Of Scope

The following remain out of scope:

* Species Clause
* Shiny Clause
* custom encounter exceptions
* balance changes
* trainer changes
* map redesigns

---

# Rule 17 — Anti-Reset Auto-Save

To stop the player from soft-resetting away a death, the game must persist a
death as soon as it happens.

Required:

* After any battle in which at least one Pokémon dies, the game must perform a
  full save (party and all PC sectors, so the Graveyard is written) before the
  player can start another battle.
* The save must capture the final post-battle state: party compaction, held-item
  transfer, the dead Pokémon in the Graveyard, and any whiteout recovery.
* The save must never corrupt the file and must keep the reloaded map intact.
* The same persistence applies when a wild encounter **newly consumes its catch
  area without a death** (Rule 16) — caught, defeated, fled, or run from. The
  area-consumed state must be saved before the player can retry the area, so a
  failed or unlucky first encounter cannot be soft-reset away.

`Implementation note:` a battle whose deaths reach the Graveyard sets a
runtime-only pending flag. The first safe overworld frame (no script, no menu,
field controls unlocked) consumes it by running an auto-save script that performs
a full normal save — checked before any approaching-trainer battle can start. The
map-view snapshot is refreshed first so the continue screen reloads correctly.
A separate runtime-only flag is set when a catch area is **newly** consumed and
drives the same first-safe-frame save through a sibling script with a neutral
message (no Graveyard wording). A death save takes priority when both are pending
in the same battle — its full save already persists the area flag — and
re-entering an already-consumed area (e.g. a blocked Poké Ball throw) does not
queue another save. Battle Frontier deaths do not use either flag; they are saved
synchronously from the lobby script (see Rule 11, including its reset-window
note). Both pending flags are cleared on Game Over so neither can leak into a
fresh New Game.

---

# Implementation Notes (Non-Rule Changes)

The shipped build contains a small number of changes that are not Nuzlocke rules
and are recorded here only to keep this document an accurate implementation
contract:

* New games default the text-speed option to Fast (instead of the base game's
  Mid). It remains fully adjustable in the Options menu. This is a
  quality-of-life default, not a balance change.
* Running Shoes work indoors (intentional, verified in-game). The running check
  ignores the per-map "running allowed" header flag that the base game uses to
  block running inside buildings. This is a quality-of-life change. All other
  running restrictions still apply, indoors and outdoors alike: metatile-based
  no-running tiles, long grass, hot springs, the Pacifidlog log bridges, and the
  low Fortree bridges remain non-runnable. Outdoor running behavior is unchanged.

No other unrelated gameplay or balance changes are present.

---

# Technical Requirements

Implementation should be conservative.

Requirements:

* Preserve save stability whenever possible.
* Avoid unrelated gameplay changes.
* Avoid unrelated balancing changes.
* Prefer small, traceable modifications.
* Document all modified files.
* Document all compromises.

If a requirement is unsafe:

Do not silently replace it.

Explain:

* the risk,
* the reason,
* the safer alternative.

## Reserved Save Flags

No new save data is introduced; the system reuses existing unused flags. These
reservations are part of the save contract and must not be reused for anything
else:

* `FLAG_UNUSED_0x020` is repurposed as the one-time Graveyard-relocation marker
  (Rule 3.2). Once set, boxes 13/14 are exclusively the Graveyard.
* The contiguous unused-flag run starting at `0x493` stores the per-catch-area
  "encounter consumed" bits for Rule 16. An area's flag is `0x493` + the area's
  index in the append-only catch-area table. With 64 catch areas, `0x493`–`0x4D2`
  are in use and `0x4D3`–`0x4EF` are reserved spares for future appended areas.
  Treat the whole `0x493`–`0x4EF` range as occupied.

`Maintenance warning:` these reservations assume the listed flags remain unused
in the base project and that `TOTAL_BOXES_COUNT` stays 14 (see Rule 3.1). After
any merge from the Revamped base or from pret upstream, re-verify that nothing
else has claimed `0x020` or any flag in `0x493`–`0x4EF`, and that the box count is
unchanged. A collision on these flags or a changed box count would corrupt
existing Nuzlocke saves.

---

# Investigation Phase Requirements (Historical)

This section records the pre-implementation process that was followed before the
Nuzlocke systems were built. The investigation is complete; these steps are
retained for provenance and are NOT a standing instruction for routine
maintenance — in particular, "wait for approval before modifying files" applied
to the original implementation effort, not to ongoing contributions.

New work that materially changes a rule (rather than fixing a bug or documenting
behavior) should still follow the same discipline that was used originally:

1. Identify all affected systems.
2. Identify all affected files.
3. Identify technical risks.
4. Identify save-data risks.
5. Identify battle-system risks.
6. Identify storage-system risks.
7. Propose an implementation plan.
8. Get the change reviewed/approved before modifying files.
