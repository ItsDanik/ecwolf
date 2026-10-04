// MiSTer hybrid core support, see mister.h

#include "mister.h"
#include "wl_iwad.h"
#include "tarray.h"
#include "zstring.h"

#include <mister_hybrid.h>

#define EXIT_TO_MENU 0
#define EXIT_RESTART 42

static const char* const TITLE = "ECWolf";

// The player came through the game list: quitting a game goes back to it
static bool PickedFromList = false;

// OSD options (CONF_STR in core/ECWolf.sv). The first entry of each is 0, the
// default.
int MiSTer_MouseSensitivity()
{
	static const int percent[16] = { 100, 125, 150, 200, 300, 400, 25, 50, 75, 100, 100, 100, 100, 100, 100, 100 };
	return percent[MH_OSD_GAME_BITS(MH_OSDStatus(), 24, 4)];
}

int MiSTer_StickSensitivity()
{
	static const int percent[16] = { 100, 125, 150, 200, 300, 25, 50, 75, 100, 100, 100, 100, 100, 100, 100, 100 };
	return percent[MH_OSD_GAME_BITS(MH_OSDStatus(), 28, 4)];
}

int MiSTer_PickIWad(WadStuff *wads, int numwads, int defaultiwad)
{
	if(!MH_Open())
		return defaultiwad;

	TArray<const char*> items;
	for(int i = 0;i < numwads;++i)
		items.Push(wads[i].Name.GetChars());
	items.Push("Exit to the MiSTer menu");

	int pick = MH_UI_Menu(TITLE, "Select a game:", &items[0], items.Size(), defaultiwad);
	if(pick < 0 || pick >= numwads)
		return -1;

	PickedFromList = true;
	MH_UI_Message(TITLE, FString("Loading ") + wads[pick].Name + "...", 0);
	return pick;
}

void MiSTer_ShowError(const char *message)
{
	if(MH_Open())
		MH_UI_Message(TITLE, message, MH_UI_WAIT|MH_UI_ERROR);
}

int MiSTer_ExitCode()
{
	// Not if we are quitting because another core was loaded
	if(PickedFromList && MH_Open())
		return EXIT_RESTART;
	return EXIT_TO_MENU;
}
