#ifndef GUARD_NUZLOCKE_H
#define GUARD_NUZLOCKE_H

// Nuzlocke core (see docs/NuzlockeSpecification.md).
//
// Death is represented purely by a Pokemon's presence in one of the two
// reserved Graveyard boxes (the last two PC boxes). No new save data is added.

// Returns TRUE if fainting in a battle of this type makes a Pokemon
// permanently dead (Rules 10, 11, 15).
bool32 Nuzlocke_BattleCountsAsDeath(u32 battleTypeFlags);

// Moves every fainted (0 HP, non-egg) party Pokemon into the Graveyard in
// death order and compacts the survivors back into the party (Rule 2).
void Nuzlocke_ProcessPartyDeaths(void);

// Called once when a battle ends, just before returning to the overworld.
// Applies permanent death if the battle type counts.
void Nuzlocke_OnBattleEnd(void);

// Battle Frontier death handling (Rule 11). Faints are recorded during a
// challenge and applied (as a script special) after the party is restored.
void Nuzlocke_RecordFrontierFaints(void);
void Nuzlocke_ApplyFrontierDeaths(void);

// One-time save-compatibility migration: relocates any living Pokemon found in
// the Graveyard boxes (13/14) of a pre-hack save into boxes 1..12 (Rule 3.2).
void Nuzlocke_InitGraveyardIfNeeded(void);

// Graveyard box queries (used by the PC storage system, Rule 3).
// The Graveyard occupies the last two PC boxes; living storage is the rest.
bool32 Nuzlocke_IsGraveyardBox(u8 boxId);  // TRUE for PC boxes 13 and 14
u8 Nuzlocke_GetFirstGraveyardBox(void);    // box index 12 ("Graveyard 1")
u8 Nuzlocke_GetLastGraveyardBox(void);     // box index 13 ("Graveyard 2")
u8 Nuzlocke_GetLivingBoxCount(void);       // number of normal boxes (12)

#endif // GUARD_NUZLOCKE_H
