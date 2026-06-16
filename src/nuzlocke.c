#include "global.h"
#include "battle.h"
#include "event_data.h"
#include "fieldmap.h"
#include "item.h"
#include "main.h"
#include "menu.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "script.h"
#include "text.h"
#include "title_screen.h"
#include "window.h"
#include "nuzlocke.h"
#include "constants/battle.h"
#include "constants/characters.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/region_map_sections.h"
#include "constants/species.h"

// One-time "Graveyard initialized" marker. Reuses an existing unused flag, so
// no new save data is introduced and the save format is unchanged. Once set,
// boxes 13/14 are exclusively the Graveyard.
#define FLAG_NUZLOCKE_GRAVEYARD_READY FLAG_UNUSED_0x020

// Rule 16: per-catch-area "encounter consumed" bits. Reuses the contiguous
// unused-flag run 0x493..0x4EF (93 flags; 64 in use, the rest reserved as
// spares), so no new save data is introduced. The flag for a catch area is
// FLAG_NUZLOCKE_CATCH_AREA_BASE + its index in sNuzlockeCatchAreaMapSecs.
#define FLAG_NUZLOCKE_CATCH_AREA_BASE FLAG_UNUSED_0x493

// Runtime-only record (never saved) of the Pokemon that fainted during the
// current Battle Frontier challenge, identified by personality value. The
// frontier heals between rounds and restores the full party at the end, so
// deaths must be remembered here and applied once the real party is back.
static u32 sFrontierDeadPersonalities[6];
static u8 sFrontierDeadCount;

// Anti-reset auto-save (runtime only, never saved). Set when battle teardown
// writes a death to the Graveyard; consumed at the first safe overworld frame
// (no script, no menu, controls unlocked), which runs the auto-save script.
// Frontier deaths do not use this flag - Nuzlocke_ApplyFrontierDeaths already
// runs from a lobby script and saves synchronously there.
static bool8 sPendingDeathAutoSave;

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

// Battle Frontier facilities that fight with the player's OWN Pokemon (Rule
// 11). Faints in these are recorded during the challenge and applied once it
// ends. Battle Factory is intentionally absent - it uses rental Pokemon, so its
// faints never count.
#define NUZLOCKE_OWN_MON_FRONTIER_BATTLES                              \
    (BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_DOME | BATTLE_TYPE_PALACE  \
   | BATTLE_TYPE_ARENA | BATTLE_TYPE_PIKE | BATTLE_TYPE_PYRAMID)

bool32 Nuzlocke_BattleCountsAsDeath(u32 battleTypeFlags)
{
    // Link battles are friendly competition; deaths there never count (Rule 10).
    if (battleTypeFlags & BATTLE_TYPE_LINK)
        return FALSE;

    // Non-lethal / special battle types never count *here*. All Battle Frontier
    // facilities (BATTLE_TYPE_FRONTIER) are excluded from immediate processing:
    // the frontier heals and restores the party between rounds, so a fainted mon
    // must not be removed mid-challenge. Instead, faints in the own-Pokemon
    // facilities are recorded during the challenge and applied at the end (see
    // Nuzlocke_OnBattleEnd / Nuzlocke_ApplyFrontierDeaths, Rule 11). Battle
    // Factory uses rental Pokemon and never counts at all.
    if (battleTypeFlags & (BATTLE_TYPE_SAFARI
                         | BATTLE_TYPE_FRONTIER
                         | BATTLE_TYPE_RECORDED
                         | BATTLE_TYPE_WALLY_TUTORIAL
                         | BATTLE_TYPE_FIRST_BATTLE))
        return FALSE;

    // Trainer and wild battles count (Rule 15).
    return TRUE;
}

// Rule 16: every map section that has a wild encounter table is one catch
// area. Multi-floor dungeons share one entry (one section), the whole Safari
// Zone is a single area, and Underwater 124/126 are separate from their
// surface routes. A catch area's flag is FLAG_NUZLOCKE_CATCH_AREA_BASE + its
// index here, and those flags live in save data - so this table is
// APPEND-ONLY: never remove, reorder or insert entries, or existing saves
// would see the wrong areas as consumed.
static const u16 sNuzlockeCatchAreaMapSecs[] =
{
    // Towns and cities with water/fishing encounters.
    MAPSEC_DEWFORD_TOWN,
    MAPSEC_PACIFIDLOG_TOWN,
    MAPSEC_PETALBURG_CITY,
    MAPSEC_SLATEPORT_CITY,
    MAPSEC_LILYCOVE_CITY,
    MAPSEC_MOSSDEEP_CITY,
    MAPSEC_SOOTOPOLIS_CITY,
    MAPSEC_EVER_GRANDE_CITY,
    // Routes.
    MAPSEC_ROUTE_101,
    MAPSEC_ROUTE_102,
    MAPSEC_ROUTE_103,
    MAPSEC_ROUTE_104,
    MAPSEC_ROUTE_105,
    MAPSEC_ROUTE_106,
    MAPSEC_ROUTE_107,
    MAPSEC_ROUTE_108,
    MAPSEC_ROUTE_109,
    MAPSEC_ROUTE_110,
    MAPSEC_ROUTE_111,
    MAPSEC_ROUTE_112,
    MAPSEC_ROUTE_113,
    MAPSEC_ROUTE_114,
    MAPSEC_ROUTE_115,
    MAPSEC_ROUTE_116,
    MAPSEC_ROUTE_117,
    MAPSEC_ROUTE_118,
    MAPSEC_ROUTE_119,
    MAPSEC_ROUTE_120,
    MAPSEC_ROUTE_121,
    MAPSEC_ROUTE_122,
    MAPSEC_ROUTE_123,
    MAPSEC_ROUTE_124,
    MAPSEC_ROUTE_125,
    MAPSEC_ROUTE_126,
    MAPSEC_ROUTE_127,
    MAPSEC_ROUTE_128,
    MAPSEC_ROUTE_129,
    MAPSEC_ROUTE_130,
    MAPSEC_ROUTE_131,
    MAPSEC_ROUTE_132,
    MAPSEC_ROUTE_133,
    MAPSEC_ROUTE_134,
    // Underwater areas (separate from their surface routes).
    MAPSEC_UNDERWATER_124,
    MAPSEC_UNDERWATER_126,
    // Caves, dungeons and special areas.
    MAPSEC_GRANITE_CAVE,
    MAPSEC_SAFARI_ZONE,
    MAPSEC_PETALBURG_WOODS,
    MAPSEC_RUSTURF_TUNNEL,
    MAPSEC_ABANDONED_SHIP,
    MAPSEC_NEW_MAUVILLE,
    MAPSEC_METEOR_FALLS,
    MAPSEC_MT_PYRE,
    MAPSEC_SHOAL_CAVE,
    MAPSEC_SEAFLOOR_CAVERN,
    MAPSEC_VICTORY_ROAD,
    MAPSEC_CAVE_OF_ORIGIN,
    MAPSEC_FIERY_PATH,
    MAPSEC_JAGGED_PASS,
    MAPSEC_SKY_PILLAR,
    MAPSEC_MAGMA_HIDEOUT,
    MAPSEC_MIRAGE_TOWER,
    MAPSEC_ARTISAN_CAVE,
    MAPSEC_DESERT_UNDERPASS,
    MAPSEC_ALTERING_CAVE,
};

// Returns the catch-area index for a map section, or -1 if the section has no
// wild encounters (towns without water, indoor maps, the Battle Frontier...).
// Maps outside the table are never consumed and never block Poke Balls, which
// also keeps scripted statics on such maps (e.g. Sudowoodo) catchable - a
// documented exception approved in the Rule 16 design review.
static s32 GetCatchAreaIndex(u8 mapsec)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sNuzlockeCatchAreaMapSecs); i++)
    {
        if (sNuzlockeCatchAreaMapSecs[i] == mapsec)
            return i;
    }
    return -1;
}

// Rule 16: the first-encounter rule is active once the rival has handed over
// the Poke Balls in Birch's Lab. FLAG_ADVENTURE_STARTED is set exactly there
// and nowhere else, so no new flag is needed. Encounters before that moment
// never consume an area.
bool32 Nuzlocke_IsCatchRuleActive(void)
{
    return FlagGet(FLAG_ADVENTURE_STARTED);
}

// TRUE if the player's current map belongs to a catch area whose first valid
// encounter has already been used up.
bool32 Nuzlocke_IsCurrentCatchAreaConsumed(void)
{
    s32 index = GetCatchAreaIndex(gMapHeader.regionMapSectionId);

    if (index < 0)
        return FALSE;
    return FlagGet(FLAG_NUZLOCKE_CATCH_AREA_BASE + index);
}

// Marks the current map's catch area as consumed. No-op on maps that are not
// catch areas.
static void ConsumeCurrentCatchArea(void)
{
    s32 index = GetCatchAreaIndex(gMapHeader.regionMapSectionId);

    if (index >= 0)
        FlagSet(FLAG_NUZLOCKE_CATCH_AREA_BASE + index);
}

// Rule 16: TRUE for battles whose end consumes the current catch area - i.e.
// real wild encounters, however they end (caught, fainted, fled, ran). Safari
// battles consume. Trainer battles never do. Excluded wild-like battles:
//  - Wally tutorial: a scripted catch; must not consume Route 102.
//  - First battle: Birch's tutorial, before Poke Balls exist.
//  - Legendary/roamer: legendaries are ignored by encounter tracking. Every
//    vanilla legendary path sets BATTLE_TYPE_LEGENDARY (statics, Regis,
//    Kyogre/Groudon, Southern Island Latis) or BATTLE_TYPE_ROAMER.
//  - Link/recorded: not real encounters.
// Frontier wild battles (Pike/Pyramid) never reach this check: OnBattleEnd
// returns early for all own-mon frontier facilities.
static bool32 IsConsumingWildBattle(u32 battleTypeFlags)
{
    if (battleTypeFlags & BATTLE_TYPE_TRAINER)
        return FALSE;
    if (battleTypeFlags & (BATTLE_TYPE_LINK
                         | BATTLE_TYPE_FIRST_BATTLE
                         | BATTLE_TYPE_WALLY_TUTORIAL
                         | BATTLE_TYPE_LEGENDARY
                         | BATTLE_TYPE_ROAMER
                         | BATTLE_TYPE_RECORDED))
        return FALSE;
    return TRUE;
}

// Rule 16: legendary Pokemon are completely banned from capture. The species
// list is a backstop: every vanilla legendary encounter already sets
// BATTLE_TYPE_LEGENDARY (statics, Regis, Kyogre/Groudon, Southern Island
// Latis) or BATTLE_TYPE_ROAMER, but a species match also blocks any future
// encounter path that forgets to set a flag.
static const u16 sNuzlockeBannedCaptureSpecies[] =
{
    SPECIES_ARTICUNO,
    SPECIES_ZAPDOS,
    SPECIES_MOLTRES,
    SPECIES_MEWTWO,
    SPECIES_MEW,
    SPECIES_RAIKOU,
    SPECIES_ENTEI,
    SPECIES_SUICUNE,
    SPECIES_LUGIA,
    SPECIES_HO_OH,
    SPECIES_CELEBI,
    SPECIES_REGIROCK,
    SPECIES_REGICE,
    SPECIES_REGISTEEL,
    SPECIES_LATIAS,
    SPECIES_LATIOS,
    SPECIES_KYOGRE,
    SPECIES_GROUDON,
    SPECIES_RAYQUAZA,
    SPECIES_JIRACHI,
    SPECIES_DEOXYS,
};

// TRUE if the Pokemon a thrown ball would target is a banned legendary.
// Wild battles are always single in vanilla Emerald, so the wild Pokemon is
// gEnemyParty[0].
bool32 Nuzlocke_IsBallTargetLegendary(void)
{
    u32 i;
    u16 species;

    if (gBattleTypeFlags & (BATTLE_TYPE_LEGENDARY | BATTLE_TYPE_ROAMER))
        return TRUE;

    species = GetMonData(&gEnemyParty[0], MON_DATA_SPECIES, NULL);
    for (i = 0; i < ARRAY_COUNT(sNuzlockeBannedCaptureSpecies); i++)
    {
        if (sNuzlockeBannedCaptureSpecies[i] == species)
            return TRUE;
    }
    return FALSE;
}

// Dupes Clause: TRUE if the player opted in at the one-time run-setup choice
// in Birch's Lab. Clear (the default, and the state of every existing save)
// means duplicates count as first encounters, exactly as before.
bool32 Nuzlocke_IsDupesClauseEnabled(void)
{
    return FlagGet(FLAG_NUZLOCKE_DUPES_CLAUSE);
}

// Dupes Clause: TRUE if a living Pokemon of this species is owned - in the
// party (non-egg, HP above zero, so a mon dying in the current battle has
// already stopped counting) or anywhere in the living boxes 1..12 (boxed mons
// have no HP; presence outside the Graveyard means alive). The Graveyard
// never counts: a species whose only members are dead may be encountered
// again. Eggs never count: their species is not yet usable.
static bool32 IsSpeciesAliveOwned(u16 species)
{
    u32 i, box, pos;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];

        if (GetMonData(mon, MON_DATA_SPECIES, NULL) != species)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;
        if (GetMonData(mon, MON_DATA_HP, NULL) == 0)
            continue;
        return TRUE;
    }

    for (box = 0; box < Nuzlocke_GetLivingBoxCount(); box++)
    {
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            struct BoxPokemon *boxMon = GetBoxedMonPtr(box, pos);

            if (GetBoxMonData(boxMon, MON_DATA_SPECIES, NULL) != species)
                continue;
            if (GetBoxMonData(boxMon, MON_DATA_IS_EGG, NULL))
                continue;
            return TRUE;
        }
    }

    return FALSE;
}

// Dupes Clause: TRUE if the current wild encounter duplicates a living owned
// Pokemon and is therefore ignored by first-encounter tracking - it cannot be
// caught and it does not consume the catch area.
bool32 Nuzlocke_IsCurrentEncounterDuplicate(void)
{
    if (!Nuzlocke_IsDupesClauseEnabled())
        return FALSE;
    return IsSpeciesAliveOwned(GetMonData(&gEnemyParty[0], MON_DATA_SPECIES, NULL));
}

// Rule 4: when a Pokemon dies, try to move its held item into the Bag so the
// player can keep using it. If the Bag has no room, the item stays attached to
// the dead Pokemon and travels to the Graveyard, where it can be retrieved
// later via the Graveyard's PC. Never destroys the item.
static void TryRetrieveHeldItemToBag(struct Pokemon *mon)
{
    u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);

    if (item != ITEM_NONE && AddBagItem(item, 1) == TRUE)
    {
        u16 none = ITEM_NONE;
        SetMonData(mon, MON_DATA_HELD_ITEM, &none);
    }
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

    // Rule 4: pull the held item into the Bag before the mon is boxed. If the
    // Bag is full the item remains on the (now boxed) mon for later retrieval.
    TryRetrieveHeldItemToBag(mon);

    // The BoxPokemon retains species, nickname, IVs/EVs, moves and any held item
    // that did not fit in the Bag - everything needed for the memorial. Current
    // HP/status are not stored for boxed mons, which is fine because a dead mon
    // is never withdrawn.
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
        // Anti-reset: persist the deaths at the first safe overworld frame.
        sPendingDeathAutoSave = TRUE;
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

// Records (by personality) every fainted, non-egg member of the current
// reduced frontier party. Idempotent across rounds (Rule 11).
void Nuzlocke_RecordFrontierFaints(void)
{
    u32 i, j;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        u32 personality;

        if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;
        if (GetMonData(mon, MON_DATA_HP, NULL) != 0)
            continue;

        personality = GetMonData(mon, MON_DATA_PERSONALITY, NULL);
        for (j = 0; j < sFrontierDeadCount; j++)
        {
            if (sFrontierDeadPersonalities[j] == personality)
                break;
        }
        if (j == sFrontierDeadCount && sFrontierDeadCount < (u8)ARRAY_COUNT(sFrontierDeadPersonalities))
            sFrontierDeadPersonalities[sFrontierDeadCount++] = personality;
    }
}

// Applies recorded frontier deaths to the now-restored full party. Called as a
// script special right after the end-of-challenge LoadPlayerParty, before the
// party is healed. Matches dead Pokemon by personality, so it is robust to the
// per-round reordering some facilities (e.g. Battle Dome) perform.
void Nuzlocke_ApplyFrontierDeaths(void)
{
    u32 i, p;
    bool32 anyDied = FALSE;

    for (p = 0; p < sFrontierDeadCount; p++)
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            struct Pokemon *mon = &gPlayerParty[i];

            if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
                continue;
            if (GetMonData(mon, MON_DATA_PERSONALITY, NULL) != sFrontierDeadPersonalities[p])
                continue;

            if (MoveMonToGraveyard(mon))
                anyDied = TRUE;
            break;
        }
    }

    sFrontierDeadCount = 0;
    if (anyDied)
    {
        CompactPartySlots();
        CalculatePlayerPartyCount();
        // Anti-reset (Rule 11): persist the deaths right here, from the lobby
        // script, before the facility's own SAVE_LINK save can write a party
        // without them. SAVE_LINK skips the PC sectors, so a reset in that
        // window would erase the dead from both party and Graveyard.
        // SaveMapView keeps the continue-screen map intact (see
        // Nuzlocke_DoDeathAutoSave).
        SaveMapView();
        TrySavingData(SAVE_NORMAL);
    }
}

void Nuzlocke_OnBattleEnd(void)
{
    // Ensure the Graveyard boxes are cleared of any pre-hack living Pokemon
    // before the first death is ever placed there.
    Nuzlocke_InitGraveyardIfNeeded();

    // Link battles never count (Rule 10).
    if (gBattleTypeFlags & BATTLE_TYPE_LINK)
        return;

    // Battle Frontier own-Pokemon facilities (Rule 11): Tower, Dome, Palace,
    // Arena, Pike and Pyramid. Record faints now; they are applied after the
    // challenge ends, once each lobby's LoadPlayerParty has restored the full
    // party and calls Nuzlocke_ApplyFrontierDeaths. Battle Factory is excluded
    // (rental Pokemon) by its absence from the mask.
    if (gBattleTypeFlags & NUZLOCKE_OWN_MON_FRONTIER_BATTLES)
    {
        Nuzlocke_RecordFrontierFaints();
        return;
    }

    // Rule 16: a finished wild encounter uses up its catch area, regardless
    // of how the battle ended. Only once the challenge has officially begun.
    // Dupes Clause: a duplicate of a living owned Pokemon is ignored by the
    // tracking entirely, leaving the area available. A catch is always valid:
    // a duplicate can never be caught (the ball is blocked), and checking
    // after a catch would wrongly see the just-caught mon as its own
    // duplicate and leave the area open.
    if (Nuzlocke_IsCatchRuleActive() && IsConsumingWildBattle(gBattleTypeFlags))
    {
        if (gBattleOutcome == B_OUTCOME_CAUGHT || !Nuzlocke_IsCurrentEncounterDuplicate())
            ConsumeCurrentCatchArea();
    }

    if (Nuzlocke_BattleCountsAsDeath(gBattleTypeFlags))
        Nuzlocke_ProcessPartyDeaths();
}

// Anti-reset auto-save consume point. Called by ProcessPlayerFieldInput on
// every frame the overworld is idle (no script running, no menu open, field
// controls unlocked) - the same dispatcher vanilla uses for field poison and
// egg hatching. Queues the auto-save script once after a battle whose deaths
// reached the Graveyard; by then any whiteout recovery, respawn and trainer
// defeat speech have already finished, so the save captures the final state.
bool32 Nuzlocke_TryQueueDeathAutoSave(void)
{
    if (!sPendingDeathAutoSave)
        return FALSE;
    sPendingDeathAutoSave = FALSE;
    ScriptContext_SetupScript(EventScript_NuzlockeDeathAutoSave);
    return TRUE;
}

// Script special for EventScript_NuzlockeDeathAutoSave: writes a full save
// (party and all PC sectors, so the Graveyard is included) while the script's
// "Saving..." message is on screen. SAVE_LINK would skip the PC.
void Nuzlocke_DoDeathAutoSave(void)
{
    // Every field save must refresh the saved map-view snapshot first
    // (start menu, Pike and Pyramid all do); on continue, LoadSavedMapView
    // pastes it back over the map around the player, so a stale snapshot
    // corrupts the loaded map's tiles.
    SaveMapView();
    TrySavingData(SAVE_NORMAL);
}

// Run-loss detection (Rule 5). The run is lost when no living, NON-EGG
// Pokemon remains in the party or in the living boxes (1..12), exactly as the
// specification defines it. Graveyard boxes (13/14) never count, and eggs
// never count: a player left with only eggs cannot battle, so the run is over
// (the eggs rest with the memorial save).
bool32 Nuzlocke_HasLivingPokemon(void)
{
    u32 i, box, pos;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];

        if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;
        return TRUE;
    }

    for (box = 0; box < Nuzlocke_GetLivingBoxCount(); box++)
    {
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            struct BoxPokemon *boxMon = GetBoxedMonPtr(box, pos);

            if (GetBoxMonData(boxMon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
                continue;
            if (GetBoxMonData(boxMon, MON_DATA_IS_EGG, NULL))
                continue;
            return TRUE;
        }
    }

    return FALSE;
}

// Rule 5 (preferred behavior): withdraws the first living, non-egg Pokemon
// found in the living boxes - scanning box 1 slot 1 through box 12 slot 30 -
// into the first free party slot. Graveyard boxes are never scanned and eggs
// are never withdrawn. Returns TRUE if a Pokemon was recovered.
static bool32 TryWithdrawFirstLivingBoxMon(void)
{
    u32 box, pos, i;

    for (box = 0; box < GRAVEYARD_BOX_1; box++)
    {
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            struct BoxPokemon *boxMon = GetBoxedMonPtr(box, pos);

            if (GetBoxMonData(boxMon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
                continue;
            if (GetBoxMonData(boxMon, MON_DATA_IS_EGG, NULL))
                continue;

            for (i = 0; i < PARTY_SIZE; i++)
            {
                if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES, NULL) == SPECIES_NONE)
                    break;
            }
            if (i == PARTY_SIZE)
                return FALSE; // No free slot (cannot happen after a wipe).

            // A Pokemon fresh from a box has full HP and no status, and the
            // whiteout's HealPlayerParty runs right after this anyway.
            BoxMonToMon(boxMon, &gPlayerParty[i]);
            ZeroBoxMonAt(box, pos);
            CompactPartySlots();
            CalculatePlayerPartyCount();
            return TRUE;
        }
    }
    return FALSE;
}

// Rule 5 (preferred behavior): called during the whiteout flow, before the
// party heal. If the wipe left the party with no usable Pokemon (empty or
// eggs only - battle deaths have already moved the fallen to the Graveyard),
// automatically recover one living boxed Pokemon so normal gameplay never
// resumes with zero usable Pokemon. If the living boxes hold nothing usable
// either, the party is left as-is and the caller's run-loss check
// (Nuzlocke_HasLivingPokemon) triggers the Game Over instead.
void Nuzlocke_TryWhiteOutPartyRecovery(void)
{
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];

        if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;
        // A non-egg party member survived (e.g. a field-poison whiteout,
        // where fainting is not a battle death); the normal heal restores it.
        return;
    }

    TryWithdrawFirstLivingBoxMon();
}

// Game Over special (Rule 5). Writes the current state as the "memorial save"
// (a normal save - never deletes or corrupts), then returns to the title
// screen. Because the saved state still has no living Pokemon, loading it again
// re-triggers the run-loss check and the Game Over, so normal play cannot
// resume from a lost run.
void Nuzlocke_SaveMemorialAndReturnToTitle(void)
{
    // A pending death auto-save must not leak past the Game Over: a New Game
    // started from the title screen without a console reset would otherwise
    // inherit it and auto-save over the old file at the first idle frame.
    sPendingDeathAutoSave = FALSE;
    // Keep the memorial's reloaded map intact (see Nuzlocke_DoDeathAutoSave).
    SaveMapView();
    TrySavingData(SAVE_NORMAL);
    SetMainCallback2(CB2_InitTitleScreen);
}

// Centred "GAME OVER" title window drawn above the Game Over dialogue.
// Placed at baseBlock 0x100 (tiles 256-287) to stay clear of the dialog
// window at 0x194 (tiles 404-511) and the pre-loaded frame tiles at 0x200.
static const struct WindowTemplate sNuzlockeGameOverTitleTemplate =
{
    .bg         = 0,
    .tilemapLeft = 9,   // centres a 12-tile window: frame at col 8..21 of 30
    .tilemapTop  = 6,   // centres the box in the space above the dialog
    .width       = 12,  // inner content = 96 px
    .height      = 2,   // inner content = 16 px (one FONT_NORMAL line)
    .paletteNum  = 15,
    .baseBlock   = 0x100,
};

static const u8 sText_NuzlockeGameOverTitle[] = _("GAME OVER");

// Colors: [foreground, background, shadow] using standard dialog palette
static const u8 sNuzlockeGameOverTitleColors[] =
{
    TEXT_COLOR_DARK_GRAY, TEXT_COLOR_WHITE, TEXT_COLOR_LIGHT_GRAY
};

// Nuzlocke (Rule 5): draw a centred "GAME OVER" title box above the dialogue.
// Called as a script special immediately before the msgbox, so the window
// is visible throughout the run-end message.
void Nuzlocke_ShowGameOverTitle(void)
{
    u8 winId;
    s32 strWidth;
    u8 x;

    // Load window-frame and text palettes into both palette buffers now so
    // the title is visible before the msgbox task reaches its palette-load
    // step (Task_DrawFieldMessage state 0).
    LoadMessageBoxAndBorderGfx();

    winId = (u8)AddWindow(&sNuzlockeGameOverTitleTemplate);
    DrawStdWindowFrame(winId, FALSE);

    // Horizontally centre "GAME OVER" in the 96 px inner area.
    strWidth = GetStringWidth(FONT_NORMAL, sText_NuzlockeGameOverTitle, 0);
    x = (u8)((96 - strWidth) / 2);

    AddTextPrinterParameterized4(winId, FONT_NORMAL, x, 0, 0, 0,
                                 sNuzlockeGameOverTitleColors, TEXT_SKIP_DRAW,
                                 sText_NuzlockeGameOverTitle);
    CopyWindowToVram(winId, COPYWIN_FULL);
}
