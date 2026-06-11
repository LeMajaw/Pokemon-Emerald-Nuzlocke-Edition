# Pokémon Emerald: Nuzlocke Edition

A Pokémon Emerald ROM hack built on the pret/pokeemerald decompilation, focused on enforcing a full Nuzlocke ruleset directly through game mechanics.

This project is not just a challenge guide. The game itself enforces the core Nuzlocke rules: permanent death, first encounter limits, forced nicknames, anti-reset saving, Graveyard storage, and run-loss behavior.

## Features

### Permanent Death

When a Pokémon faints in a valid battle, it permanently dies.

Dead Pokémon:

* cannot battle again;
* cannot be healed back into usability;
* cannot be revived;
* cannot be withdrawn from the Graveyard;
* cannot be traded or moved back into normal gameplay.

### Graveyard PC

Dead Pokémon are preserved in a dedicated Graveyard system instead of being deleted.

* Boxes 13 and 14 are reserved as Graveyard boxes.
* Normal living storage uses boxes 1 through 12.
* Graveyard boxes are accessed through a dedicated Graveyard's PC entry.
* Dead Pokémon are stored in death order.
* Dead Pokémon cannot be moved, withdrawn, renamed, or restored.
* Held items can still be recovered safely.

### First Encounter Per Area

The game enforces the classic first-encounter rule.

After the Nuzlocke officially begins:

* only the first valid encounter in each catch area may be caught;
* running, fainting, fleeing, or ending the battle consumes the area;
* later Poké Ball attempts in that area are blocked;
* the Poké Ball is not consumed when blocked;
* catch areas persist through save/load.

Supported area logic includes:

* routes;
* caves;
* water routes;
* underwater areas;
* Safari Zone;
* multi-floor dungeons as shared areas.

### Nuzlocke Activation

The first-encounter rule starts only after the rival gives the player Poké Balls.

Encounters before that point do not consume areas.

### Forced Nicknames

Every Pokémon obtained by the player must receive a nickname.

This applies to:

* wild captures;
* starter Pokémon;
* gift Pokémon;
* fossil revivals;
* egg hatches;
* NPC trades.

Empty nicknames are rejected.

### Legendary Capture Ban

Legendary Pokémon cannot be caught.

* Poké Balls are blocked against legendary Pokémon.
* Master Ball does not bypass the restriction.
* Roaming Latios/Latias are also blocked.
* Legendary encounters do not consume catch areas.

### Auto-Save After Death

To reduce reset abuse, the game automatically saves after a Pokémon is moved to the Graveyard.

The auto-save preserves:

* the dead Pokémon in the Graveyard;
* party compaction;
* held item transfer;
* whiteout recovery state when applicable;
* map state correctly after reload.

### Whiteout Recovery

If the active party wipes but living Pokémon remain in storage:

* the first available living non-Egg Pokémon from boxes 1 through 12 is automatically withdrawn;
* normal whiteout recovery continues safely;
* the player never resumes with zero usable Pokémon.

### Game Over / Memorial Save

If no living non-Egg Pokémon remain anywhere:

* the run ends;
* a GAME OVER screen appears;
* a memorial save is created;
* reloading the save returns to the Game Over state.

### Held Item Recovery

When a Pokémon dies:

* the game attempts to move its held item into the Bag;
* if the Bag is full, the item remains with the dead Pokémon;
* the item can later be recovered through the Graveyard PC;
* item recovery never restores or moves the dead Pokémon.

### Revive Removal

Revival mechanics are disabled.

* Revive is removed or replaced where obtainable.
* Max Revive is removed or replaced where obtainable.
* Sacred Ash cannot revive Pokémon.
* Revive-style item effects are disabled.
* Revives are removed from shops and normal gameplay access.

### Day Care Disabled

The Day Care is disabled to prevent passive leveling and breeding.

* Pokémon cannot be deposited.
* Breeding is blocked.
* Gift eggs still work normally.
* Hatched Pokémon must be nicknamed.

### Trade Rules

NPC trades are allowed.

Player-to-player Pokémon trades are blocked.

Link battles remain allowed for fun, and link battle faints do not count as deaths.

### Battle Frontier Support

Deaths count in Battle Frontier facilities that use the player's real Pokémon:

* Battle Tower
* Battle Dome
* Battle Palace
* Battle Arena
* Battle Pike
* Battle Pyramid

Battle Factory is excluded because it uses rental Pokémon.

### PC Storage Changes

Normal storage boxes remain usable for living Pokémon.

* Someone's PC accesses boxes 1 through 12 only.
* Graveyard's PC accesses Graveyard boxes only.
* Graveyard boxes use the Simple wallpaper.
* Simple wallpaper is reserved for the Graveyard.
* Normal boxes retain standard wallpaper behavior.

## Rules Summary

1. Every Pokémon must be nicknamed.
2. Fainted Pokémon permanently die.
3. Dead Pokémon go to the Graveyard.
4. Only the first valid encounter per catch area may be caught.
5. Legendary Pokémon cannot be caught.
6. Revives and revival mechanics are disabled.
7. Day Care is disabled.
8. NPC trades are allowed.
9. Player-to-player trades are blocked.
10. Link battle deaths do not count.
11. Battle Frontier own-Pokémon facilities count.
12. Battle Factory does not count.
13. Whiteout may recover from living storage.
14. No living Pokémon anywhere means Game Over.
15. Deaths trigger anti-reset auto-save.

## Build Notes

This project is based on the Pokémon Emerald decompilation by pret.

The project is intended to be built from source. No ROM files are included.

Use the normal pokeemerald build process for your environment.

For this branch, gameplay changes intentionally modify the ROM, so `make compare` is not expected to match vanilla Emerald.

Use:

```bash
make -j4
```

## Repository Structure

Important branches:

* `nuzlocke` — main Nuzlocke Edition branch.
* `vanilla-verified` — verified vanilla reference branch.
* `upstream-master` — upstream pret reference branch.

## Version

Current release:

```txt
v1.0.0
```

## Credits

Based on the pret Pokémon Emerald decompilation project.

This repository contains source code changes for a Nuzlocke-focused ROM hack and does not include copyrighted ROM files.
