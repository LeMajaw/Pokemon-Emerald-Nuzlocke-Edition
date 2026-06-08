#include "global.h"
#include "battle.h"
#include "event_data.h"
#include "item.h"
#include "main.h"
#include "menu.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "text.h"
#include "title_screen.h"
#include "window.h"
#include "nuzlocke.h"
#include "constants/battle.h"
#include "constants/characters.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/species.h"

// One-time "Graveyard initialized" marker. Reuses an existing unused flag, so
// no new save data is introduced and the save format is unchanged. Once set,
// boxes 13/14 are exclusively the Graveyard.
#define FLAG_NUZLOCKE_GRAVEYARD_READY FLAG_UNUSED_0x020

// Runtime-only record (never saved) of the Pokemon that fainted during the
// current Battle Frontier challenge, identified by personality value. The
// frontier heals between rounds and restores the full party at the end, so
// deaths must be remembered here and applied once the real party is back.
static u32 sFrontierDeadPersonalities[6];
static u8 sFrontierDeadCount;

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

    if (Nuzlocke_BattleCountsAsDeath(gBattleTypeFlags))
        Nuzlocke_ProcessPartyDeaths();
}

// Run-loss detection (Rule 5). The run is lost only when no Pokemon at all
// remain in the party or in the living boxes (1..12). Graveyard boxes (13/14)
// never count. Eggs count as "not yet lost" since they can still hatch.
bool32 Nuzlocke_HasLivingPokemon(void)
{
    u32 i, box, pos;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE)
            return TRUE;
    }

    for (box = 0; box < Nuzlocke_GetLivingBoxCount(); box++)
    {
        for (pos = 0; pos < IN_BOX_COUNT; pos++)
        {
            if (GetBoxMonData(GetBoxedMonPtr(box, pos), MON_DATA_SPECIES, NULL) != SPECIES_NONE)
                return TRUE;
        }
    }

    return FALSE;
}

// Game Over special (Rule 5). Writes the current state as the "memorial save"
// (a normal save - never deletes or corrupts), then returns to the title
// screen. Because the saved state still has no living Pokemon, loading it again
// re-triggers the run-loss check and the Game Over, so normal play cannot
// resume from a lost run.
void Nuzlocke_SaveMemorialAndReturnToTitle(void)
{
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
