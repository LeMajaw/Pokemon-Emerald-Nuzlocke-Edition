# Difficulty Specification

Version: 1.2.0

This document is the source of truth for this hack's **difficulty systems**: the
Recommended Level, modern party-wide EXP, Over-Cap EXP restriction, and the
Momentum system. It is a companion to `NuzlockeSpecification.md` (which remains
the source of truth for the Nuzlocke ruleset) and documents the **behavior as
actually implemented in code**, not the original design discussion.

Primary implementation: `src/difficulty.c` + `include/difficulty.h`, wired into
`src/battle_script_commands.c` (`Cmd_getexp`) and a few hook sites. Everything
here is **runtime-only — no save data is added**.

> **Implemented vs. future.** Sections 1–9 describe shipped, in-code behavior.
> Section 11 lists known limitations. Section 12 lists ideas that are **not yet
> implemented**.

---

## 0. Philosophy
A Pokémon above the intended level band can still grow, but only through
sustained risk in meaningful battles — *risk → reward*, never *restriction →
punishment*. The world is fixed: there is **no trainer or boss scaling**, no
disobedience, and EXP is never set to a punitive zero for a Pokémon that actually
fights. The player always chooses whether to exceed the recommendation.

---

## 1. Recommended Levels
Every major progression point has a recommended level, taken from this fork's
boss ace levels (`sRecommendedLevels[]` in `difficulty.c`):

| Index | Boss / milestone | Recommended level |
|---|---|---|
| 0 | Roxanne (Gym 1) | 15 |
| 1 | Brawly (Gym 2) | 19 |
| 2 | Wattson (Gym 3) | 24 |
| 3 | Flannery (Gym 4) | 29 |
| 4 | Norman (Gym 5) | 31 |
| 5 | Winona (Gym 6) | 33 |
| 6 | Tate & Liza (Gym 7) | 42 |
| 7 | Juan (Gym 8) | 46 |
| 8 | League (Elite Four → Champion) | 58 |

- **Active recommendation** = `sRecommendedLevels[badgeCount]`, where `badgeCount`
  is the number of Badges earned (counted from `FLAG_BADGE01_GET`…`08`). So with 0
  Badges the cap is 15 (heading to Roxanne); with 4 Badges it is 31 (heading to
  Norman); with 8 Badges it is **58** for the whole League gauntlet (there is no
  normal Pokémon Center break between Elite Four members, so the band targets the
  Champion's level, not the first E4 member).
- **A Pokémon is "over-cap"** when its level exceeds the active recommendation.
  `delta = monLevel − recommendedLevel` (0 if at or under). Only over-cap Pokémon
  are affected by Sections 3–7; everything at or below the band behaves like the
  base game.

*Example:* with 4 Badges (cap 31), a Lv31 mon is at-cap (delta 0), a Lv33 mon is
over by 2 (delta 2).

---

## 2. Modern Party-Wide EXP
EXP is awarded to **every living, non-egg party Pokémon**, not just the ones sent
out, using the modern (Gen VII+/IX) level-scaled formula. Each receiver's amount
is computed **independently** in `Cmd_getexp` (`CalcModernExp`), so EXP is never
split between participants:

```
EXP = (b × L) / (5 × s) × ((2L + 10)^2.5 / (L + Lp + 10)^2.5) + 1
```

- `b` = fainted mon's base EXP yield; `L` = fainted mon's level; `Lp` = the
  receiver's **own** level.
- `s = 1` for a **participant** (full share); `s = 2` for a **passive** party
  member (half share). There is **no** division by the number of participants —
  each participant receives its own full amount (Gen VI+ behaviour).
- The `Lp` term is the catch-up: a lower-level member gains **more** than a same-
  level member from the same KO; an over-level member gains less.
- `^2.5` is computed in integers as `x² × Sqrt(x)` (GBA BIOS `Sqrt`), with a u64
  intermediate so the product cannot overflow.
- Vanilla boosts are preserved on top, applied in order: **Lucky Egg ×1.5**,
  **trainer battle ×1.5**, **traded/outsider ×1.5**.

**Eligibility / exclusions:**
- Eggs and fainted (0 HP) Pokémon receive nothing.
- Max-level Pokémon receive nothing.
- A participant that **fainted before the foe did** receives nothing (0 HP is
  excluded; in a counting Nuzlocke battle it is dead and headed to the Graveyard).
- A passive bench mon receives nothing when the KO-er is an **over-cap sweeper**
  (Section 3a), so an overtrained Pokémon cannot passively carry the team.

**The Exp Share item is redundant and removed from obtainability.** Party-wide EXP
is always on, so a held Exp Share is a no-op. As of v1.2.0 it is no longer given by
the Devon Corp reward (now the Link Stone) or the Lottery Corner; its constant,
data, and hold effect remain defined but are unobtainable in normal play.

---

## 3. Over-Cap EXP Restriction
The over-cap cap is applied **last** — after modern level scaling (Section 2) and
the Lucky Egg / trainer / traded multipliers — so the catch-up term can never push
an over-cap Pokémon past the cap. For an over-cap Pokémon (`delta > 0`):

- **Did not participate (passive)** → **0**. An over-cap Pokémon is excluded from
  passive Party EXP Share entirely: it is filtered out in `Cmd_getexp` *before*
  recipients are determined, and may only gain EXP by participating in battle.
  Momentum is never read or built for a non-participant.
- **Participated** → reduced EXP using the base table, indexed by `delta`
  (`sOverCapBaseRate[]`), and Momentum may lift sub-50% brackets (participants
  only):

| Levels over cap (`delta`) | Base EXP rate |
|---|---|
| +1 | 80% |
| +2 | 60% |
| +3 | 40% |
| +4 | 25% |
| +5 or more | 10% |

- The final amount is `baseExp × finalRate / 100`, and is **never reduced to a
  literal 0** for a participant (minimum 1). `baseExp` here is the full vanilla
  amount *after* the Lucky Egg / trainer / traded boosts.

*Example:* cap 31, a Lv34 participant (delta 3) earns 40% of what it normally
would (before Momentum).

### 3a. Over-Cap KO suppresses party-share
An over-leveled Pokémon that does the work must not passively train the rest of
the team. When the Pokémon that **lands the KO** is itself over-cap:

- **non-participants receive 0** passive party EXP from that KO (the party base is
  withheld), and
- the over-cap KO-er still earns its own reduced EXP per Section 3, and any
  **under-cap co-participant** still earns its normal participant share.

If the KO-er is at or under the cap, party-wide EXP behaves exactly as in
Section 2. The KO-er is identified from the move user on the normal faint path
(`gBattlerAttacker`); a foe that faints to recoil, status, or its own action has
no player KO-er, so the suppression does not apply and EXP is shared normally.

*Example:* cap 31, a Lv40 starter KOs the foe while a Lv20 catch and four benched
mons are alive. The starter earns its reduced participant EXP; the bench earns
**nothing**. Had a Lv31 mon landed the KO instead, the whole party would share
normally.

---

## 4. Momentum (overview)
Momentum is a per-Pokémon value (0…1000 internal points) that **only matters for
over-cap Pokémon**. It rewards continuing to use an over-leveled Pokémon in real,
meaningful battles without resetting its resources. It is:

- **Runtime-only.** Stored in EWRAM (`sMomentum[]`), keyed by the Pokémon's
  **personality value** (so it survives party reordering). **It is never written
  to the save file** — it resets to 0 whenever the game is loaded.
- **Not a streak/combo counter.** It is weighted by how meaningful each victory
  is (Section 5), and it collides naturally with HP/PP/status attrition
  (Section 7), so the intended optimal play is "keep adventuring," not "farm one
  route."

---

## 5. Momentum Gain
Granted in `Cmd_getexp` to each over-cap **participant** when a foe faints:

```
levelFactor = clamp( (opponentLevel − (userLevel − 15)) × 10 , 0 , 100 )   // 0..100
gain        = levelFactor × importance × 25 / 100
momentum    = min( 1000 , momentum + gain )
```

- `levelFactor` is **full (100)** when the foe is within ~5 levels below the user
  and tapers to **0** by ~15 levels below — so weak wild Pokémon give nothing.
- **Importance** (set from the battle type / trainer class):

| Opponent | Importance |
|---|---|
| Wild Pokémon | 1 |
| Regular trainer | 2 |
| Gym Leader (`TRAINER_CLASS_LEADER`) | 3 |
| Elite Four / Champion (`TRAINER_CLASS_ELITE_FOUR` / `CHAMPION`) | 4 |

*Worked examples (Lv36 user, over-cap):*
- vs **Lv5 Zigzagoon (wild):** `levelFactor = (5 − 21)×10 < 0 → 0` ⇒ **+0**.
- vs **Lv31 trainer mon:** `levelFactor = (31 − 21)×10 = 100`; `gain = 100×2×25/100 =` **50**.
- vs a **~Lv33 Gym ace:** `100×3×25/100 =` **75**.
- vs a **Lv50 Elite Four mon:** `100×4×25/100 =` **100**.

So roughly 10 Elite-Four-grade KOs, ~14 Gym-grade, or ~20 trainer-grade meaningful
KOs fill the bar (tunable via `MOMENTUM_GAIN_SCALE`).

---

## 6. How Momentum Affects EXP
Momentum can only **lift over-cap EXP rates that are below 50%**, and never above
50% (Interpretation A):

```
momentumPercent = momentum × 40 / 1000          // 0..40 percentage points
if baseRate >= 50:  finalRate = baseRate         // +1 (80%) and +2 (60%) are UNAFFECTED
else:               finalRate = min(baseRate + momentumPercent, 50)
```

- The **+1 (80%)** and **+2 (60%)** brackets are already ≥ 50%, so Momentum does
  nothing to them — being only slightly over-cap is always the better rate.
- The **+3 (40%)**, **+4 (25%)**, and **+5+ (10%)** brackets can be lifted by
  Momentum up to a hard ceiling of **50%**, never higher.

*Example:* a Lv36 mon at cap 31 (delta 5) earns 10% base. With a full Momentum bar
(1000 → +40 points) it earns `min(10 + 40, 50) =` **50%** — its best possible rate.
A delta-3 mon (40% base) reaches the 50% ceiling with only +10 points of Momentum.

---

## 7. Momentum Loss
| Event | Effect on that Pokémon's Momentum |
|---|---|
| Pokémon Center / full-party heal (`HealPlayerParty`) | **reset to 0** (entire party) |
| Faint/death → moved to the Graveyard | **reset to 0** |
| Boxed / removed from the party | **reset to 0** |
| HP-recovery item used on it | **× 50%** (retain 50%) |
| PP-restoration item used on it | **× 50%** |
| Pure status-cure item used on it | **× 75%** (retain 75%) |

Notes:
- **Full Restore** heals HP, so it is treated as an HP-recovery item → **×50%**
  (not the status rate).
- Item reductions apply to **both field and in-battle** item use, because in-battle
  medicine/PP use is routed through the same party-menu callbacks
  (`ItemUseCB_Medicine` / `TryUsePPItem`) — the reduction fires exactly once per
  item use, in either context. **AI/opponent item use does not affect the player's
  Momentum.**
- Non-HP/PP/status items (X-items, EV berries, Rare Candy, etc.) do **not** change
  Momentum.
- Because heal/box/death reset to 0 and the only way a fainted Pokémon returns to
  usability in this hack is a Center heal (revives are disabled), a fainted
  Pokémon's Momentum is always cleared before it could matter again.

---

## 8. Gym Statue Displays
Each Gym certification statue shows that Gym's recommended level (hard-coded to
match the Section 1 table):

- **Before earning the Badge:** the Gym name, then `LEAGUE RECOMMENDED LEVEL:` and
  the number.
- **After earning the Badge:** the existing certified-trainers text (including the
  player name) is preserved, **plus** the recommended-level lines.

Statues are present in six Gyms: **Rustboro (15), Dewford (19), Mauville (24),
Petalburg (31), Fortree (33), Mossdeep (42)**. **Lavaridge and Sootopolis Gyms
have no certification statues in the base game**, so their recommended levels
(29 and 46) are not shown on a statue (see Section 12).

---

## 9. End-to-End Example
Player has **4 Badges** (active cap **31**) and a Lv36 starter (delta 5, base
rate 10%) plus a benched Lv28 mon.

1. The player fights a route trainer. The Lv28 benched mon is at/under cap → it
   receives full party EXP. The Lv36 starter, if it **participated**, earns 10% ×
   (any Momentum lift); if it **stayed benched**, it earns **0**.
2. The starter sweeps several near-level trainer Pokémon → Momentum climbs
   (+50 per meaningful KO), lifting its rate from 10% toward the **50%** ceiling.
3. The player uses a Potion on the starter mid-route → its Momentum is **halved**.
4. The player reaches a Pokémon Center and heals → all Momentum **resets to 0**;
   the starter is back to the 10% base rate until it earns Momentum again.

The benched Lv28 mon levels normally throughout (it is under-cap), helping the
player keep a balanced, in-band team without grinding it separately.

---

## 10. Save Compatibility
This system adds **no save data**. The Recommended Level is derived from Badge
flags; Momentum lives only in EWRAM and is never serialized. Existing save files
are fully compatible, and turning the system off (Section 13) restores base-game
EXP behavior with no save impact.

---

## 11. Known Limitations (implemented behavior)
- **Momentum has no in-game readout yet.** Players infer it from EXP behavior
  across battles. (A summary-screen gauge is planned — Section 12.)
- **Momentum resets on game load.** Because it is runtime-only, saving and
  reloading clears it (treated as a between-session "cool-down").
- **Lavaridge and Sootopolis recommended levels are not displayed** (those Gyms
  have no statue objects).
- **Exp Share item is redundant** (party-wide EXP is always on).
- **Momentum is keyed by personality value;** a collision between two party
  Pokémon with the same personality is astronomically unlikely and, if it
  occurred, would only share a Momentum entry (harmless).
- **Recommended levels are static constants.** If boss ace levels change, update
  both `sRecommendedLevels[]` and the Gym statue text.
- **In-battle item Momentum reduction targets the chosen Pokémon** via the shared
  party-menu callback; this is intended and matches field behavior.

---

## 12. Future / Not Yet Implemented
The following are **not in the code** and are listed only as direction:
- A **summary-screen Momentum gauge** (and recommended-level/over-cap indicator),
  shown only for over-cap Pokémon.
- Displaying recommended levels for **Lavaridge and Sootopolis** (e.g., via their
  Gym Guide NPC dialogue) since they lack statues.
- **Persisting Momentum** across saves (would require save space; deliberately
  avoided for now).
- The broader **smarter-trainer / AI-tier** and **personality-based adaptive boss**
  systems are separate future work and are **not** part of this implementation.

---

## 13. Tuning & Toggle (for contributors)
All knobs live in `src/difficulty.c`:
- `sRecommendedLevels[]` — the per-milestone band (keep in sync with Gym statue text).
- `sOverCapBaseRate[]` — `{100, 80, 60, 40, 25, 10}` (index = `delta`, 1..5).
- `MOMENTUM_MAX` (1000), `MOMENTUM_BONUS_MAX` (40), `MOMENTUM_RATE_CEIL` (50),
  `MOMENTUM_GAIN_SCALE` (25).
- `DIFFICULTY_SYSTEM_ENABLED` (in `include/difficulty.h`) — set to `FALSE` to fully
  revert to base-game EXP behavior.

## 14. Source Map
- `include/difficulty.h`, `src/difficulty.c` — recommended levels, over-cap EXP,
  Momentum store + formulas, master toggle.
- `src/battle_script_commands.c` (`Cmd_getexp`) — party-wide split, over-cap
  scaling (`Difficulty_ScaleExp`), Momentum gain (`Difficulty_AddMomentumForKO`)
  + importance detection.
- `src/script_pokemon_util.c` (`HealPlayerParty`) — `Difficulty_ResetAllMomentum`.
- `src/nuzlocke.c` (`Nuzlocke_ProcessPartyDeaths`) — reset on death.
- `src/pokemon_storage_system.c` (`TryStorePartyMonInBox`) — reset on boxing.
- `src/party_menu.c` (`ItemUseCB_Medicine`, `TryUsePPItem`) — item reductions
  (field and in-battle).
- `src/item_use.c` (`ItemUseInBattle_Medicine`, `_PPRecovery`) — comments only,
  documenting the shared-callback routing.
- Gym statue text — `data/maps/{Rustboro,Dewford,Mauville,Petalburg,Fortree,Mossdeep}City_Gym/scripts.inc`
  (and `DewfordTown_Gym`, `LavaridgeTown_Gym` naming aside).
