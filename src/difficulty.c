#include "global.h"
#include "difficulty.h"
#include "event_data.h"
#include "pokemon.h"
#include "constants/flags.h"

// Difficulty systems (Recommended Level + Over-Cap EXP + Momentum).
// See docs/DifficultySpecification.md. Runtime-only: this file adds NO save data.

// Recommended level per milestone, set from this fork's boss ace levels:
//  0 Roxanne 15, 1 Brawly 19, 2 Wattson 24, 3 Flannery 29, 4 Norman 31,
//  5 Winona 33, 6 Tate&Liza 42, 7 Juan 46, 8 League/Champion band 58.
// The Gym statue for gym G (1-based) uses index G-1; the active cap uses the
// badge count, so 0 badges -> next boss is Roxanne (15) and 8 badges -> the
// League band (58). APPEND/EDIT carefully if boss levels change.
static const u8 sRecommendedLevels[NUM_BADGES + 1] =
{
    15, 19, 24, 29, 31, 33, 42, 46, 58,
};

// Over-cap base EXP rate by how many levels above the cap (index = delta).
// Index 0 (at/under cap) is full and handled before this table is read.
static const u8 sOverCapBaseRate[6] = { 100, 80, 60, 40, 25, 10 };

// Momentum tuning (all runtime-only, freely re-balanceable here).
#define MOMENTUM_MAX          1000  // internal points (full bar)
#define MOMENTUM_BONUS_MAX    40    // max percent points Momentum can add
#define MOMENTUM_RATE_CEIL    50    // Momentum may only lift rates below this
#define MOMENTUM_GAIN_SCALE   25    // points gained per fully-meaningful KO unit

// Runtime Momentum store, keyed by personality value (robust to party reorder,
// never saved). Mirrors the per-mon-by-personality pattern already used by the
// Nuzlocke frontier-death tracker. An entry with momentum == 0 is "empty".
static struct
{
    u32 personality;
    u16 momentum;
} sMomentum[PARTY_SIZE];

// --- Recommended level -------------------------------------------------------

static u8 CountBadges(void)
{
    u8 i, count = 0;

    for (i = 0; i < NUM_BADGES; i++)
    {
        if (FlagGet(FLAG_BADGE01_GET + i))
            count++;
    }
    return count;
}

u8 Difficulty_GetRecommendedLevel(void)
{
    return sRecommendedLevels[CountBadges()]; // CountBadges() is 0..NUM_BADGES
}

u8 Difficulty_GetGymRecommendedLevel(u8 gymIndex)
{
    if (gymIndex >= NUM_BADGES)
        gymIndex = NUM_BADGES - 1;
    return sRecommendedLevels[gymIndex];
}

u8 Difficulty_GetOverCapDelta(struct Pokemon *mon)
{
    u8 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
    u8 rec = Difficulty_GetRecommendedLevel();

    if (level <= rec)
        return 0;
    return (u8)(level - rec);
}

// --- Momentum store helpers --------------------------------------------------

static s32 FindMomentumSlot(u32 personality)
{
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (sMomentum[i].momentum != 0 && sMomentum[i].personality == personality)
            return (s32)i;
    }
    return -1;
}

static s32 FindOrCreateMomentumSlot(u32 personality)
{
    u32 i;
    s32 slot = FindMomentumSlot(personality);

    if (slot >= 0)
        return slot;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (sMomentum[i].momentum == 0)
        {
            sMomentum[i].personality = personality;
            sMomentum[i].momentum = 0;
            return (s32)i;
        }
    }
    return -1; // full (cannot happen for <= PARTY_SIZE distinct mons)
}

static u32 MomentumToPercent(u16 momentum)
{
    return (u32)momentum * MOMENTUM_BONUS_MAX / MOMENTUM_MAX;
}

u16 Difficulty_GetMomentum(struct Pokemon *mon)
{
    s32 slot;

    if (!DIFFICULTY_SYSTEM_ENABLED)
        return 0;

    slot = FindMomentumSlot(GetMonData(mon, MON_DATA_PERSONALITY, NULL));
    return (slot < 0) ? 0 : sMomentum[slot].momentum;
}

void Difficulty_AddMomentumForKO(struct Pokemon *mon, u8 opponentLevel, u8 importance)
{
    u8 level, rec;
    s32 levelFactor, gain, slot;
    u32 personality, momentum;

    if (!DIFFICULTY_SYSTEM_ENABLED)
        return;

    level = GetMonData(mon, MON_DATA_LEVEL, NULL);
    rec = Difficulty_GetRecommendedLevel();
    if (level <= rec) // only over-cap mons accrue Momentum
        return;

    // levelFactor 0..100: full when the opponent is within ~5 levels below the
    // user, tapering to 0 by ~15 levels below (so weak wild mons give nothing).
    levelFactor = ((s32)opponentLevel - ((s32)level - 15)) * 10;
    if (levelFactor <= 0)
        return;
    if (levelFactor > 100)
        levelFactor = 100;

    gain = levelFactor * (s32)importance * MOMENTUM_GAIN_SCALE / 100;
    if (gain <= 0)
        return;

    personality = GetMonData(mon, MON_DATA_PERSONALITY, NULL);
    slot = FindOrCreateMomentumSlot(personality);
    if (slot < 0)
        return;

    momentum = (u32)sMomentum[slot].momentum + (u32)gain;
    if (momentum > MOMENTUM_MAX)
        momentum = MOMENTUM_MAX;
    sMomentum[slot].personality = personality;
    sMomentum[slot].momentum = (u16)momentum;
}

void Difficulty_ReduceMomentumForMon(struct Pokemon *mon, u8 percentRetained)
{
    s32 slot = FindMomentumSlot(GetMonData(mon, MON_DATA_PERSONALITY, NULL));

    if (slot < 0)
        return;
    sMomentum[slot].momentum = (u16)((u32)sMomentum[slot].momentum * percentRetained / 100);
}

void Difficulty_ResetMomentumByPersonality(u32 personality)
{
    s32 slot = FindMomentumSlot(personality);

    if (slot < 0)
        return;
    sMomentum[slot].personality = 0;
    sMomentum[slot].momentum = 0;
}

void Difficulty_ResetMomentumForMon(struct Pokemon *mon)
{
    Difficulty_ResetMomentumByPersonality(GetMonData(mon, MON_DATA_PERSONALITY, NULL));
}

void Difficulty_ResetAllMomentum(void)
{
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        sMomentum[i].personality = 0;
        sMomentum[i].momentum = 0;
    }
}

// --- Over-cap EXP ------------------------------------------------------------

u32 Difficulty_ScaleExp(u32 baseExp, struct Pokemon *mon, bool32 participated)
{
    u8 delta;
    u32 baseRate, finalRate, exp;

    if (!DIFFICULTY_SYSTEM_ENABLED)
        return baseExp;

    delta = Difficulty_GetOverCapDelta(mon);
    if (delta == 0)
        return baseExp;      // at/under cap: unchanged (full party EXP applies)

    if (delta > 5)
        delta = 5;
    baseRate = sOverCapBaseRate[delta];

    // Over-cap passive (non-participant) EXP is reduced by the SAME table, but
    // never reads or builds Momentum - Momentum is earned only by fighting, so
    // passive bench EXP can neither gain nor refresh it.
    if (!participated)
    {
        finalRate = baseRate;
    }
    else if (baseRate >= MOMENTUM_RATE_CEIL)
    {
        finalRate = baseRate;   // bracket already >= 50%: Momentum has no effect
    }
    else
    {
        finalRate = baseRate + MomentumToPercent(Difficulty_GetMomentum(mon));
        if (finalRate > MOMENTUM_RATE_CEIL)
            finalRate = MOMENTUM_RATE_CEIL;
    }

    exp = baseExp * finalRate / 100;
    if (exp == 0 && baseExp != 0)
        exp = 1;                // never literally zero for an eligible receiver
    return exp;
}
