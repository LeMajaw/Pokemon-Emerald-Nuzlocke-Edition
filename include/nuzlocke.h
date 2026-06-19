#ifndef GUARD_NUZLOCKE_H
#define GUARD_NUZLOCKE_H

// Nuzlocke core (see docs/NuzlockeSpecification.md).
//
// Death is represented purely by a Pokemon's presence in one of the two
// reserved Graveyard boxes (the last two PC boxes). No new save data is added.

// Returns TRUE if fainting in a battle of this type is processed immediately.
// Some own-Pokemon Battle Frontier deaths are handled separately after party
// restoration (Rules 10, 11, 15).
bool32 Nuzlocke_BattleCountsAsDeath(u32 battleTypeFlags);

// Moves every fainted (0 HP, non-egg) party Pokemon into the Graveyard in
// death order and compacts the survivors back into the party (Rule 2).
void Nuzlocke_ProcessPartyDeaths(void);

// Called once when a battle ends, just before returning to the overworld.
// Applies or records permanent death if the battle context counts.
void Nuzlocke_OnBattleEnd(void);

// Battle Frontier death handling (Rule 11). Faints are recorded during a
// challenge and applied (as a script special) after the party is restored.
void Nuzlocke_RecordFrontierFaints(void);
void Nuzlocke_ApplyFrontierDeaths(void);

// Run-loss / memorial save (Rule 5).
// TRUE if any living, non-egg Pokemon remains in the party or living boxes
// (1..12). Eggs and Graveyard Pokemon never count.
bool32 Nuzlocke_HasLivingPokemon(void);
// Whiteout auto-recovery (Rule 5 preferred behavior): if the party has no
// usable (non-egg) Pokemon, withdraws the first living non-egg Pokemon from
// boxes 1..12 (box order, then slot order) into the party.
void Nuzlocke_TryWhiteOutPartyRecovery(void);
// Game Over special: write the memorial save and return to the title screen.
void Nuzlocke_SaveMemorialAndReturnToTitle(void);
// Game Over title special: draws a centred "GAME OVER" window above the msgbox.
void Nuzlocke_ShowGameOverTitle(void);

// Anti-reset auto-save. Battle deaths set a runtime-only pending flag; the
// first idle overworld frame consumes it by running the auto-save script
// (full SAVE_NORMAL, so the Graveyard's PC sectors are written).
bool32 Nuzlocke_TryQueueDeathAutoSave(void); // hook in ProcessPlayerFieldInput
void Nuzlocke_DoDeathAutoSave(void);         // script special: TrySavingData
extern const u8 EventScript_NuzlockeDeathAutoSave[];
extern const u8 EventScript_NuzlockeAreaAutoSave[];

// Game Over field script (shown when a run is lost).
extern const u8 EventScript_NuzlockeGameOver[];

// gFieldCallback that runs the Game Over script on field entry (Rule 5).
void FieldCB_NuzlockeGameOver(void);

// One-time save-compatibility migration: relocates any living Pokemon found in
// the Graveyard boxes (13/14) of a pre-hack save into boxes 1..12 (Rule 3.2).
void Nuzlocke_InitGraveyardIfNeeded(void);

// First-encounter rule (Rule 16). Active once the rival has handed over the
// Poke Balls (FLAG_ADVENTURE_STARTED). Each map section with wild encounters
// is one catch area; a finished wild battle consumes it permanently.
bool32 Nuzlocke_IsCatchRuleActive(void);
// TRUE if the current map's catch area has already used its first encounter.
bool32 Nuzlocke_IsCurrentCatchAreaConsumed(void);
// TRUE if a thrown ball would target a banned legendary (Rule 16).
bool32 Nuzlocke_IsBallTargetLegendary(void);
// Dupes Clause (optional, chosen once at the run-setup question).
bool32 Nuzlocke_IsDupesClauseEnabled(void);
// TRUE if the current wild encounter duplicates a living owned Pokemon:
// uncatchable, and it does not consume the catch area.
bool32 Nuzlocke_IsCurrentEncounterDuplicate(void);

// Graveyard box queries (used by the PC storage system, Rule 3).
// The Graveyard occupies the last two PC boxes; living storage is the rest.
bool32 Nuzlocke_IsGraveyardBox(u8 boxId);  // TRUE for PC boxes 13 and 14
u8 Nuzlocke_GetFirstGraveyardBox(void);    // box index 12 ("Graveyard 1")
u8 Nuzlocke_GetLastGraveyardBox(void);     // box index 13 ("Graveyard 2")
u8 Nuzlocke_GetLivingBoxCount(void);       // number of normal boxes (12)

#endif // GUARD_NUZLOCKE_H
