@ Nuzlocke (Rule 5): run-loss / Game Over. Shown when no living Pokemon remain
@ in the party or the living boxes (1-12). Writes the memorial save and returns
@ to the title screen; the lost run can never resume normal play.
EventScript_NuzlockeGameOver::
	lockall
	special Nuzlocke_ShowGameOverTitle
	msgbox gText_NuzlockeGameOver, MSGBOX_DEFAULT
	special Nuzlocke_SaveMemorialAndReturnToTitle
	waitstate
	end

gText_NuzlockeGameOver:
	.string "All of your POKéMON have fallen.\p"
	.string "Your Nuzlocke journey has come to\n"
	.string "an end…\p"
	.string "Their memory lives on in the\n"
	.string "GRAVEYARD.$"

@ Nuzlocke anti-reset auto-save. Queued by Nuzlocke_TryQueueDeathAutoSave at
@ the first idle overworld frame after a battle whose deaths reached the
@ Graveyard. No button press needed: print, save, jingle, close.
EventScript_NuzlockeDeathAutoSave::
	lockall
	message gText_NuzlockeDeathAutoSave
	waitmessage
	special Nuzlocke_DoDeathAutoSave
	playse SE_SAVE
	waitse
	delay 30
	closemessage
	releaseall
	end

gText_NuzlockeDeathAutoSave:
	.string "The fallen rest in the GRAVEYARD.\n"
	.string "Saving… don't turn off the power.$"
