@ Nuzlocke (Rule 5): run-loss / Game Over. Shown when no living Pokemon remain
@ in the party or the living boxes (1-12). Writes the memorial save and returns
@ to the title screen; the lost run can never resume normal play.
EventScript_NuzlockeGameOver::
	lockall
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
