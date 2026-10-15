#ifndef __MISTER_H__
#define __MISTER_H__

// MiSTer hybrid core: the game runs on the ARM, the ECWolf FPGA core shows
// the picture at 15kHz and provides audio and input. SDL's "mister"
// drivers (hybrid/sdl2 in the MiSTer-ecwolf repository) do most of the work;
// this is what is specific to the game.

struct WadStuff;

// Joystick buttons as SDL sees them: the buttons of the core's "J1," list
// (ECWolf.sv) in order, then the buttons the SDL driver adds
enum
{
	MISTER_JOY_ATTACK,
	MISTER_JOY_USE,
	MISTER_JOY_RUN,
	MISTER_JOY_NEXTWEAPON,
	MISTER_JOY_PREVWEAPON,
	MISTER_JOY_STRAFE,
	MISTER_JOY_MAP,
	MISTER_JOY_MENU,
	MISTER_JOY_CORE_OK,
	MISTER_JOY_CORE_BACK,

	MISTER_JOY_MENU_OK = 28,
	MISTER_JOY_MENU_BACK = 29
};
#define MISTER_JOY_OK_MASK ((1 << MISTER_JOY_CORE_OK) | (1 << MISTER_JOY_MENU_OK))
#define MISTER_JOY_BACK_MASK ((1 << MISTER_JOY_CORE_BACK) | (1 << MISTER_JOY_MENU_BACK))

// Bumped whenever the default controls change: the controls saved by an older
// version are then replaced by the new defaults once (c_cvars.cpp)
#define MISTER_CONTROLS_VERSION 2

// OSD options: how fast the mouse and the stick turn, in percent, on top of
// the game's own sensitivity settings
int MiSTer_MouseSensitivity();
int MiSTer_StickSensitivity();

// OSD option Resolution: the size of the screen the core is to show, 320 or
// 640 pixels in each of 200 or 240 lines. The picture is 4:3 at every size
void MiSTer_Resolution(unsigned &width, unsigned &height);
// Follows the Resolution option, and in 240 lines the HUD margin: true if
// the size of the screen or the place of the status bar has just changed,
// and what is on it has to be drawn again
bool MiSTer_UpdateResolution();

// 240 lines (hybrid/CLAUDE.md, "240 lines and HUD margins"). The 200 lines of
// the game's 2D graphics are not stretched to the screen but centred on it;
// the view fills the screen from the top edge to the bottom one, and the
// status bar keeps its size and is drawn over it, the player's HUD margin
// (OSD option HUD (Bottom)) above the bottom edge.
//
// The margin in lines of a 240-line screen, as the layout was last made for;
// SetViewSize() reads it anew (`read`)
int MiSTer_HudMargin(bool read = false);
// Set while the status bar's graphics are drawn (wolf_sbar.cpp)
extern bool MiSTer_HudDrawing;
// Where line 0 of the 200 virtual lines goes on a 240-line screen: 20 for
// what is centred, and for the status bar its margin above the bottom
int MiSTer_VirtualTop();

// Frame pacing. The core shows 59.6 fields per second and the game runs 70
// tics per second, so a frame is not a whole number of tics. Waits for the
// next field and returns the whole tics that have passed since the last call;
// `frac` (0..FRACUNIT-1) is how far the picture to draw is into the tic after
// those, which the renderer interpolates with. False without the core.
bool MiSTer_FrameTics(unsigned &tics, int &frac);
// The next call starts counting anew, after the game stood still
void MiSTer_ResetFrameTics();

// Cheats page of the OSD: God Mode on (1) or off (0), and the triggers
// (MH_OSDTriggers(), T[56] and T[57]) selected since the last call, once a
// frame while the game is played (wl_debug.cpp)
int MiSTer_GodMode();
enum { MISTER_CHEAT_GIVEALL = 1, MISTER_CHEAT_NEXTLEVEL = 2 };
int MiSTer_CheatTriggers();

// The game to start (MH_PickGame()) of the ones ECWolf found, by their
// iwadinfo Autoname: the menu entry's, or the player's pick from a list.
// Returns the index, -1 to quit (after a message when none is there)
int MiSTer_PickIWad(WadStuff *wads, const char *const *autonames, int numwads);
// Shows the error on the screen until the player leaves the core
void MiSTer_ShowError(const char *message);
// Exit code of the process after the player quit: the launcher starts the
// game again (back to the game list) or returns to the MiSTer menu
int MiSTer_ExitCode();

// The music's chip (an AdLib card's YM3812) is in the core since host version
// 10 of the hybrid framework: the game sends its register writes there and
// does not work the chip out itself. OSD option FM Music: FPGA or Software.
//
// After the audio device has been opened. True when the core has the chip.
bool MiSTer_FMStart();
// Once at the start of every block of audio the game makes (with the audio
// mutex held): follows the OSD option. True when the chip in the core is the
// one that is heard, and the game's own need not make any sound.
bool MiSTer_FMUpdate();
// A register write (with the audio mutex held). `volume` is the game's
// volume setting the write is made under (0..MAX_VOLUME): it is the level of
// the notes started on that channel from now on, as in the game's own chip.
// `ahead` is how many audio frames into the block being made it belongs.
void MiSTer_FMWrite(unsigned reg, unsigned val, const int &volume, int ahead);

#endif
