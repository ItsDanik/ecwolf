// MiSTer hybrid core support, see mister.h

#include "mister.h"
#include "wl_def.h"
#include "c_cvars.h"
#include "id_vl.h"
#include "id_vh.h"
#include "wl_main.h"
#include "wl_play.h"
#include "wl_iwad.h"
#include "tarray.h"
#include "zstring.h"

#include <mister_hybrid.h>
#include <math.h>
#include <string.h>

#define EXIT_TO_MENU 0
#define EXIT_RESTART 42 // danik_hybrid_launch.sh starts the game again
#define EXIT_RELOADED 43 // the same, after the core has settled

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

int MiSTer_GodMode()
{
	return MH_OSD_GAME_BITS(MH_OSDStatus(), 35, 1);
}

int MiSTer_CheatTriggers()
{
	return MH_OSDTriggers();
}

void MiSTer_Resolution(unsigned &width, unsigned &height)
{
	// 320x200, 640x200, 320x240, 640x240. 320x200 as well when there is no
	// core to ask; 200 lines when the core is an older one without the mode
	static const int modes[4] = { MH_MODE_320x200, MH_MODE_640x200, MH_MODE_320x240, MH_MODE_640x240 };
	int mode = MH_Open() ? modes[MH_OSD_GAME_BITS(MH_OSDStatus(), 32, 2)] : MH_MODE_320x200;
	if(!MH_ModeAvailable(mode))
		mode = MH_ModeWidth(mode) == 640 ? MH_MODE_640x200 : MH_MODE_320x200;
	width = MH_ModeWidth(mode);
	height = MH_ModeHeight(mode);
	// the HUD (Bottom) option is greyed out in 200 lines
	MH_SetMenuMask(height == 200 ? 1 : 0);
}

bool MiSTer_UpdateResolution()
{
	unsigned width, height;
	MiSTer_Resolution(width, height);
	if(width == screenWidth && height == screenHeight)
	{
		// the status bar follows the HUD margin
		const int margin = MiSTer_HudMargin();
		if(height % 240 != 0 || MiSTer_HudMargin(true) == margin)
			return false;
		NewViewSize(viewsize);
		return true;
	}

	screenWidth = fullScreenWidth = windowedScreenWidth = width;
	screenHeight = fullScreenHeight = windowedScreenHeight = height;
	r_ratio = static_cast<Aspect>(CheckRatio(screenWidth, screenHeight));
	VH_Startup(); // Recalculate fizzlefade stuff.
	VL_SetVGAPlaneMode();
	return true;
}

bool MiSTer_HudDrawing = false;
static int HudMargin = MH_HUD_DEFAULT;

int MiSTer_HudMargin(bool read)
{
	if(read)
	{
		int top;
		MH_HudMargins(&top, &HudMargin);
	}
	return HudMargin;
}

int MiSTer_VirtualTop()
{
	return MiSTer_HudDrawing ? 40 - HudMargin : 20;
}

static uint32_t LastField;
static uint32_t TicAccum; // 16.16
static bool FieldValid = false;

void MiSTer_ResetFrameTics()
{
	FieldValid = false;
}

bool MiSTer_FrameTics(unsigned &tics, int &frac)
{
	if(!MH_IsOpen())
		return false;

	if(!FieldValid)
	{
		LastField = MH_FieldCounter();
		TicAccum = 0;
		FieldValid = true;
	}
	MH_WaitField(LastField);
	const uint32_t field = MH_FieldCounter();
	uint32_t fields = field - LastField;
	LastField = field;
	if(fields == 0) // the core is gone
		return false;
	if(fields > MAXTICS)
		fields = MAXTICS;

	// A field is 262 lines of 400 pixels at 6.25MHz (hybrid_host.sv)
	const uint32_t ticsPerField = (uint32_t)(((uint64_t)TICRATE * 400 * 262 << FRACBITS) / 6250000);
	TicAccum += fields * ticsPerField;
	tics = TicAccum >> FRACBITS;
	TicAccum &= FRACUNIT - 1;
	frac = TicAccum;
	return true;
}

// The games with a menu entry of their own (package/games.txt), and the
// shareware and demo their entries start when the full game is not there:
// ECWolf's game (iwadinfo.txt, Autoname) as the core's set name ECWolf_<id>.
// The 3-episode Wolfenstein (*.WL3) is ECWolf's "Wolf.Wolf3D.Registered" too.
struct MiSTerGame
{
	const char *autoname;
	const char *id;
	const char *name;
	const char *shareware_of;
	const char *pattern; // its files, for a game that was not found
	const char *missing;
};
static const MiSTerGame Games[] =
{
	{ "Wolf.Wolf3D.Registered", "WL", "Wolfenstein 3D", NULL, "*.WL6",
		"Wolfenstein 3D was not found. Copy its files (*.WL6 or *.WL3, or *.WL1 "
		"of the shareware) to %s/ on the SD card." },
	{ "Wolf.Wolf3D.Shareware", "WL1", "Wolfenstein 3D Shareware", "WL", NULL, NULL },
	{ "Wolf.Spear.Commercial", "SOD", "Spear of Destiny", NULL, "*.SOD",
		"Spear of Destiny was not found. Copy its files (*.SOD, or *.SDM of the "
		"demo) to %s/ on the SD card." },
	{ "Wolf.Spear.Demo", "SDM", "Spear of Destiny Demo", "SOD", NULL, NULL },
	{ "Wolf.Spear.Danger", "SD2", "SoD: Return to Danger", NULL, "*.SD2",
		"Return to Danger was not found. Copy its files (*.SD2) and those of "
		"Spear of Destiny (*.SOD) to %s/ on the SD card." },
	{ "Wolf.Spear.Challenge", "SD3", "SoD: Ultimate Challenge", NULL, "*.SD3",
		"Ultimate Challenge was not found. Copy its files (*.SD3) and those of "
		"Spear of Destiny (*.SOD) to %s/ on the SD card." },
};
#define NO_GAME "No game data found. Copy the files of Wolfenstein 3D (*.WL6, *.WL3, " \
	"*.WL1) or Spear of Destiny (*.SOD, *.SD2, *.SD3, *.SDM) to %s/ on the SD card."

int MiSTer_PickIWad(WadStuff *wads, const char *const *autonames, int numwads)
{
	// The games ECWolf found, each found (no files to look for), then the
	// games of a menu entry it did not find, with their message. A game found
	// twice (a level set) has the menu entry the first time only.
	TArray<mh_game> games;
	TArray<int> wad;
	TArray<FString> ids;
	static const char *const none[] = { NULL };
	bool seen[countof(Games)] = {};
	for(int i = 0;i < numwads;++i)
		ids.Push(autonames[i]);
	for(int i = 0;i < numwads;++i)
	{
		mh_game game = { ids[i].GetChars(), wads[i].Name.GetChars(), none, NULL, NULL };
		for(unsigned g = 0;g < countof(Games);++g)
		{
			if(strcmp(autonames[i], Games[g].autoname) == 0 && !seen[g])
			{
				seen[g] = true;
				game.id = Games[g].id;
				game.name = Games[g].name;
				game.shareware_of = Games[g].shareware_of;
			}
		}
		games.Push(game);
		wad.Push(i);
	}
	static const char *files[countof(Games)][2];
	for(unsigned g = 0;g < countof(Games);++g)
	{
		if(seen[g] || Games[g].pattern == NULL)
			continue;
		files[g][0] = Games[g].pattern;
		files[g][1] = NULL;
		mh_game game = { Games[g].id, Games[g].name, files[g], NULL, Games[g].missing };
		games.Push(game);
		wad.Push(-1);
	}

	int listed = 0;
	int pick = MH_PickGame(games.Size() ? &games[0] : NULL, games.Size(), NULL, NO_GAME, &listed);
	if(pick < 0)
		return -1;
	if(wad[pick] < 0)
	{
		// some of its files are there, not all of them
		FString text;
		text.Format(games[pick].missing, MH_GamePath());
		MiSTer_ShowError(text);
		return -1;
	}
	PickedFromList = listed != 0;
	if(MH_Open())
		MH_UI_Message(TITLE, FString("Loading ") + games[pick].name + "...", 0);
	return wad[pick];
}

void MiSTer_ShowError(const char *message)
{
	if(MH_Open())
		MH_UI_Message(TITLE, message, MH_UI_WAIT|MH_UI_ERROR);
}

int MiSTer_ExitCode()
{
	// The player loaded the core again while the game ran
	if(MH_CoreReloaded())
		return EXIT_RELOADED;
	// Not if we are quitting because another core was loaded
	if(PickedFromList && MH_Open())
		return EXIT_RESTART;
	return EXIT_TO_MENU;
}

// ---------------------------------------------------------------------------
// The FM chip in the core

static bool FMPresent = false;
static bool FMOn = false;
// what the game has written, whichever chip is heard
static uint8_t FMReg[256];
// The game's own chip scales a channel by the volume setting its last note
// was started under, and follows that setting while the note sounds. The
// real chip has one output: here the level is taken off the operators that
// are heard (0.75dB a step of their "total level").
static const int *FMChanVolume[9];
static int FMChanSteps[9];
static const uint8_t FMChanOps[9][2] = { {0,3}, {1,4}, {2,5}, {8,11}, {9,12}, {10,13}, {16,19}, {17,20}, {18,21} };

static int FMSteps(int channel)
{
	if(!FMChanVolume[channel])
		return 0;
	int v = *FMChanVolume[channel];
	v = v < 0 ? 0 : v > 20 ? 20 : v;
	// as MULTIPLY_VOLUME (id_sd.h)
	return (int)(-20.0 * log10((v + 0.3) / 20.3) / 0.75 + 0.5);
}

// The value for the chip of a register as the game wrote it
static unsigned FMValue(unsigned reg)
{
	unsigned val = FMReg[reg];
	if(reg >= 0x40 && reg <= 0x55 && (reg & 7) < 6)
	{
		const unsigned op = reg - 0x40;
		const unsigned channel = (op >> 3) * 3 + (op & 7) % 3;
		// the second operator is always heard, the first one only when the
		// two are added and not one modulating the other
		if((op & 7) >= 3 || (FMReg[0xC0 + channel] & 1))
		{
			unsigned level = (val & 0x3F) + FMChanSteps[channel];
			val = (val & 0xC0) | (level > 0x3F ? 0x3F : level);
		}
	}
	return val;
}

static void FMSend(unsigned reg, int ahead)
{
	MH_FMWrite(reg, FMValue(reg), ahead);
}

static void FMSendLevels(unsigned channel, int ahead)
{
	FMSend(0x40 + FMChanOps[channel][0], ahead);
	FMSend(0x40 + FMChanOps[channel][1], ahead);
}

bool MiSTer_FMStart()
{
	memset(FMReg, 0, sizeof(FMReg));
	memset(FMChanVolume, 0, sizeof(FMChanVolume));
	memset(FMChanSteps, 0, sizeof(FMChanSteps));
	FMOn = false;
	FMPresent = MH_Open() && MH_FMAvailable();
	if(FMPresent)
	{
		MH_FMVolume(0);
		MH_FMEnable(1);
	}
	return FMPresent;
}

bool MiSTer_FMUpdate()
{
	if(!FMPresent)
		return false;

	const bool want = MH_OSD_GAME_BITS(MH_OSDStatus(), 34, 1) == 0;
	if(want != FMOn)
	{
		FMOn = want;
		if(!FMOn)
		{
			// the chip plays on unheard, and gets no more writes
			MH_FMVolume(0);
			return false;
		}
		// everything the game has written so far, the notes last
		static const unsigned char order[] = { 0x01, 0x08, 0xBD, 0x20, 0x60, 0x80, 0xE0, 0x40, 0xC0, 0xA0, 0xB0 };
		for(unsigned i = 0;i < sizeof(order);i++)
		{
			const unsigned first = order[i];
			const unsigned count = first < 0x20 || first == 0xBD ? 1 : first >= 0xA0 && first != 0xE0 ? 9 : 22;
			for(unsigned reg = first;reg < first + count;reg++)
			{
				if(count == 22 && (reg & 7) > 5)
					continue;
				FMSend(reg, 0);
			}
		}
		// four times the chip's own level, as the game's own chip is mixed
		// (id_sd.cpp, YM3812UpdateOne)
		MH_FMVolume(127);
	}
	if(!FMOn)
		return false;

	for(unsigned channel = 0;channel < 9;channel++)
	{
		const int steps = FMSteps(channel);
		if(steps != FMChanSteps[channel])
		{
			FMChanSteps[channel] = steps;
			FMSendLevels(channel, 0);
		}
	}
	return true;
}

void MiSTer_FMWrite(unsigned reg, unsigned val, const int &volume, int ahead)
{
	reg &= 0xFF;
	FMReg[reg] = val;

	if((reg >= 0xA0 && reg <= 0xA8) || (reg >= 0xB0 && reg <= 0xB8))
	{
		// the channel takes the volume of whoever plays it now
		const unsigned channel = reg & 0xF;
		FMChanVolume[channel] = &volume;
		const int steps = FMSteps(channel);
		if(steps != FMChanSteps[channel])
		{
			FMChanSteps[channel] = steps;
			if(FMOn)
				FMSendLevels(channel, ahead);
		}
	}
	if(!FMOn)
		return;
	FMSend(reg, ahead);
	// which operators are heard has changed
	if(reg >= 0xC0 && reg <= 0xC8)
		FMSend(0x40 + FMChanOps[reg - 0xC0][0], ahead);
}
