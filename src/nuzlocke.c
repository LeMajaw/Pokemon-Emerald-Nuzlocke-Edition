#include "global.h"
#include "battle.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "nuzlocke.h"
#include "constants/battle.h"
#include "constants/species.h"

// Nuzlocke core implementation. See docs/NuzlockeSpecification.md.
//
// The Graveyard occupies the last two PC boxes. Living storage uses the
// remaining boxes (1..12). "Dead" means "lives in a Graveyard box" - there is
// no extra save data, so existing saves remain compatible.
#define GRAVEYARD_BOX_1 (TOTAL_BOXES_COUNT - 2) // PC box 13
#define GRAVEYARD_BOX_2 (TOTAL_BOXES_COUNT - 1) // PC box 14

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

void Nuzlocke_OnBattleEnd(void)
{
    if (Nuzlocke_BattleCountsAsDeath(gBattleTypeFlags))
        Nuzlocke_ProcessPartyDeaths();
}
