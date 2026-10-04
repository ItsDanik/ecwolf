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

// OSD option Resolution: the size of the screen the core is to show, 320x200
// or 640x200. The picture is 4:3 at both sizes
void MiSTer_Resolution(unsigned &width, unsigned &height);
// Follows the Resolution option: true if the size of the screen has just
// changed, and what is on it has to be drawn again
bool MiSTer_UpdateResolution();

// Frame pacing. The core shows 59.6 fields per second and the game runs 70
// tics per second, so a frame is not a whole number of tics. Waits for the
// next field and returns the whole tics that have passed since the last call;
// `frac` (0..FRACUNIT-1) is how far the picture to draw is into the tic after
// those, which the renderer interpolates with. False without the core.
bool MiSTer_FrameTics(unsigned &tics, int &frac);
// The next call starts counting anew, after the game stood still
void MiSTer_ResetFrameTics();

// Lets the player pick the game on the screen. Returns the index, -1 to quit
int MiSTer_PickIWad(WadStuff *wads, int numwads, int defaultiwad);
// Shows the error on the screen until a button is pressed
void MiSTer_ShowError(const char *message);
// Exit code of the process after the player quit: the launcher starts the
// game again (back to the game list) or returns to the MiSTer menu
int MiSTer_ExitCode();

#endif
