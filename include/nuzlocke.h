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

#endif // GUARD_NUZLOCKE_H
