#ifndef GUARD_DIFFICULTY_H
#define GUARD_DIFFICULTY_H

// Difficulty systems: Recommended Level + Over-Cap EXP + Momentum.
// Source of truth: docs/DifficultySpecification.md.
//
// Design summary:
//  - Every progression point has a Recommended Level (the next major boss's
//    band, derived from badges). Pokemon AT OR BELOW it behave exactly as in
//    the base game. Only Pokemon ABOVE it are affected.
//  - Over-cap Pokemon receive reduced EXP (base table +1=80% .. +5=10%) and no
//    passive party EXP - they must participate in battle to gain anything.
//  - Momentum (runtime only, never saved) can lift sub-50% over-cap rates up to
//    50%, rewarding sustained, meaningful battles without resetting resources.
//
// This whole system adds NO save data; Momentum lives only in EWRAM.

struct Pokemon;

// Master toggle. Set to FALSE to fully revert to base-game EXP behavior.
#define DIFFICULTY_SYSTEM_ENABLED   TRUE

// Opponent importance for Momentum gain (chosen by the EXP caller from the
// battle type / trainer class). Higher = more meaningful victory.
#define DIFFICULTY_OPP_WILD     1
#define DIFFICULTY_OPP_TRAINER  2
#define DIFFICULTY_OPP_GYM      3
#define DIFFICULTY_OPP_ELITE    4

// --- Recommended level -------------------------------------------------------
// Active recommendation (the next major progression boss), from badges earned.
u8 Difficulty_GetRecommendedLevel(void);
// The recommendation for a specific gym (0 = Rustboro/Roxanne .. 7 = Sootopolis/Juan).
// Used by the Gym statues.
u8 Difficulty_GetGymRecommendedLevel(u8 gymIndex);
// Levels a Pokemon is above the active recommendation (0 if at or under it).
u8 Difficulty_GetOverCapDelta(struct Pokemon *mon);

// --- Over-cap EXP ------------------------------------------------------------
// Final EXP for a mon given the base-game amount and whether it participated.
//  - At/under cap        -> baseExp unchanged (modern passive party EXP applies).
//  - Over cap, no part.  -> 0 (no passive party EXP; must expose to danger).
//  - Over cap, particip. -> baseExp * finalRate, where finalRate = over-cap base
//                           table, lifted by Momentum but never above 50% for
//                           sub-50% brackets; never literally 0 for a participant.
u32 Difficulty_ScaleExp(u32 baseExp, struct Pokemon *mon, bool32 participated);

// --- Momentum (runtime only, keyed by personality; never saved) --------------
// Only meaningful for over-cap Pokemon.
u16 Difficulty_GetMomentum(struct Pokemon *mon);
// Grant Momentum for contributing to a KO of an opponent (no-op if at/under cap
// or if the opponent was too weak to be meaningful).
void Difficulty_AddMomentumForKO(struct Pokemon *mon, u8 opponentLevel, u8 importance);
// Multiply a mon's Momentum by percentRetained/100 (HP/PP items = 50, status-only = 75).
void Difficulty_ReduceMomentumForMon(struct Pokemon *mon, u8 percentRetained);
// Clear one mon's Momentum (faint, removed from party).
void Difficulty_ResetMomentumForMon(struct Pokemon *mon);
// Clear Momentum for a personality directly (used when boxing a Pokemon).
void Difficulty_ResetMomentumByPersonality(u32 personality);
// Clear all Momentum (Pokemon Center heal).
void Difficulty_ResetAllMomentum(void);

#endif // GUARD_DIFFICULTY_H
