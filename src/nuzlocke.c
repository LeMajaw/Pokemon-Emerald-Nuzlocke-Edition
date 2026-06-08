#include "global.h"
#include "battle.h"
#include "event_data.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "nuzlocke.h"
#include "constants/battle.h"
#include "constants/flags.h"
#include "constants/species.h"

// One-time "Graveyard initialized" marker. Reuses an existing unused flag, so
// no new save data is introduced and the save format is unchanged. Once set,
// boxes 13/14 are exclusively the Graveyard.
#define FLAG_NUZLOCKE_GRAVEYARD_READY FLAG_UNUSED_0x020

// Nuzlocke core implementation. See docs/NuzlockeSpecification.md.
//
// The Graveyard occupies the last two PC boxes. Living storage uses the
// remaining boxes (1..12). "Dead" means "lives in a Graveyard box" - there is
// no extra save data, so existing saves remain compatible.
#define GRAVEYARD_BOX_1 (TOTAL_BOXES_COUNT - 2) // PC box 13 ("Graveyard 1")
#define GRAVEYARD_BOX_2 (TOTAL_BOXES_COUNT - 1) // PC box 14 ("Graveyard 2")

bool32 Nuzlocke_IsGraveyardBox(u8 boxId)
{
    return (boxId == GRAVEYARD_BOX_1 || boxId == GRAVEYARD_BOX_2);
}

u8 Nuzlocke_GetFirstGraveyardBox(void)
{
    return GRAVEYARD_BOX_1;
}

u8 Nuzlocke_GetLastGraveyardBox(void)
{
    return GRAVEYARD_BOX_2;
}

u8 Nuzlocke_GetLivingBoxCount(void)
{
    return TOTAL_BOXES_COUNT - 2;
}

bool32 Nuzlocke_BattleCountsAsDeath(u32 battleTypeFlags)
{
    // Link battles are friendly competition; deaths there never count (Rule 10).
    if (battleTypeFlags & BATTLE_TYPE_LINK)
        return FALSE;

    // Non-lethal / special battle types never count. NOTE: Battle Tower (part
    // of BATTLE_TYPE_FRONTIER) is intentionally excluded here for now. Per Rule
    // 11 it *should* count, but the frontier heals and restores the party
    // between rounds and tracks fixed party slots, so removing a fainted mon
    // mid-challenge risks corrupting facility state. Battle Tower death must be
    // applied at end-of-challenge instead - flagged as a follow-up.
    if (battleTypeFlags & (BATTLE_TYPE_SAFARI
                         | BATTLE_TYPE_FRONTIER
                         | BATTLE_TYPE_RECORDED
                         | BATTLE_TYPE_WALLY_TUTORIAL
                         | BATTLE_TYPE_FIRST_BATTLE))
        return FALSE;

    // Trainer and wild battles count (Rule 15).
    return TRUE;
}

// Moves a single fainted Pokemon into the Graveyard (box 13 first, then box
// 14) in death order. Returns TRUE if placed (and cleared from the party slot).
static bool32 MoveMonToGraveyard(struct Pokemon *mon)
{
    s16 slot;
    u8 boxId = GRAVEYARD_BOX_1;

    slot = GetFirstFreeBoxSpot(boxId);
    if (slot < 0)
    {
        boxId = GRAVEYARD_BOX_2;
        slot = GetFirstFreeBoxSpot(boxId);
    }
    if (slot < 0)
        return FALSE; // Graveyard full (60 dead); leave the mon in place.

    // The BoxPokemon retains species, nickname, IVs/EVs, moves and held item -
    // everything needed for the memorial. Current HP/status are not stored for
    // boxed mons, which is fine because a dead mon is never withdrawn.
    SetBoxMonAt(boxId, slot, &mon->box);
    ZeroMonData(mon);
    return TRUE;
}

void Nuzlocke_ProcessPartyDeaths(void)
{
    u32 i;
    bool32 anyDied = FALSE;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];

        if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;
        if (GetMonData(mon, MON_DATA_HP, NULL) != 0)
            continue;

        // A non-egg party Pokemon at 0 HP fainted in a counting battle: it dies.
        if (MoveMonToGraveyard(mon))
            anyDied = TRUE;
    }

    if (anyDied)
    {
        CompactPartySlots();
        CalculatePlayerPartyCount();
    }
}

// Moves a single boxed Pokemon into the first free slot of a living box
// (0..11). Returns TRUE on success.
static bool32 TryRelocateToLivingBox(struct BoxPokemon *src)
{
    u32 box;
    s16 slot;

    for (box = 0; box < GRAVEYARD_BOX_1; box++)
    {
        slot = GetFirstFreeBoxSpot(box);
        if (slot >= 0)
        {
            SetBoxMonAt(box, slot, src);
            return TRUE;
        }
    }
    return FALSE;
}

// Save-compatibility relocation (Rule 3.2). Runs exactly once, before any dead
// Pokemon is ever placed in the Graveyard. Any *living* Pokemon found in boxes
// 13/14 in a pre-hack save is moved to the first free slot in boxes 1..12, so
// the Graveyard boxes start empty and no living Pokemon is ever stranded or
// turned into a memorial.
void Nuzlocke_InitGraveyardIfNeeded(void)
{
    u32 box, pos;

    if (FlagGet(FLAG_NUZLOCKE_GRAVEYARD_READY))
        return;

    for (box = GRAVEYARD_BOX_1; box <= GRAVEYARD_BOX_2; box++)
    {
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            struct BoxPokemon *mon = GetBoxedMonPtr(box, pos);

            if (GetBoxMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
                continue;

            if (TryRelocateToLivingBox(mon))
                ZeroBoxMonAt(box, pos);
            // If boxes 1..12 are somehow completely full (360 slots), leave the
            // mon in place rather than delete it - never strand or lose a mon.
        }
    }

    FlagSet(FLAG_NUZLOCKE_GRAVEYARD_READY);
}

void Nuzlocke_OnBattleEnd(void)
{
    // Ensure the Graveyard boxes are cleared of any pre-hack living Pokemon
    // before the first death is ever placed there.
    Nuzlocke_InitGraveyardIfNeeded();

    if (Nuzlocke_BattleCountsAsDeath(gBattleTypeFlags))
        Nuzlocke_ProcessPartyDeaths();
}
