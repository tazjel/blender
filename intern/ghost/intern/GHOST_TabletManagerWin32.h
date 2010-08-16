// safe & friendly WinTab wrapper
// by Mike Erwin, July 2010

#ifndef GHOST_TABLET_MANAGER_WIN32_H
#define GHOST_TABLET_MANAGER_WIN32_H

#define _WIN32_WINNT 0x501 // require Windows XP or newer
#define WIN32_LEAN_AND_MEAN
#include	<windows.h>
#include "wintab.h"
#include <map>
#include <vector>

#include "GHOST_Types.h"
class GHOST_WindowWin32;

// BEGIN from Wacom's Utils.h
typedef UINT ( API * WTINFOA ) ( UINT, UINT, LPVOID );
typedef HCTX ( API * WTOPENA ) ( HWND, LPLOGCONTEXTA, BOOL );
typedef BOOL ( API * WTCLOSE ) ( HCTX );
typedef BOOL ( API * WTQUEUESIZESET ) ( HCTX, int );
typedef int  ( API * WTPACKETSGET ) ( HCTX, int, LPVOID );
typedef BOOL ( API * WTPACKET ) ( HCTX, UINT, LPVOID );
// END

// TabletToolData (and its components) are meant to replace GHOST_TabletData
// and its customdata analogue in the window manager. For now it's confined to the
// TabletManager.

typedef enum { TABLET_NONE, TABLET_PEN, TABLET_ERASER, TABLET_MOUSE } TabletToolType;

typedef struct
	{
	TabletToolType type : 4; // plenty of room to grow

	// capabilities
	bool hasPressure : 1;
	bool hasTilt : 1;

	} TabletTool;


typedef struct
	{
	TabletTool tool;

	float pressure;
	float tilt_x, tilt_y;

	} TabletToolData;


// "Tablet" structure is not active by pencils down
// but will be soon.

struct Tablet
	{
	bool hasPressure;
	float pressureScale;

	bool hasTilt;
	float azimuthScale;
	float altitudeScale;

	UINT size_x, size_y;

	UINT cursorCount;
	UINT cursorBase;

	WTPKT allTools; // standard info available from any tool (mouse/pen/etc.)
	WTPKT someTools; // extra info available from only some tools

	bool ownsCursor(UINT x)
		{ return (x - cursorBase) < cursorCount; }
	};

class GHOST_TabletManagerWin32
	{
	// the Wintab library
	HMODULE lib_Wintab;

	// WinTab function pointers
	WTOPENA func_Open;
	WTCLOSE func_Close;
	WTINFOA func_Info;
	WTQUEUESIZESET func_QueueSizeSet;
	WTPACKETSGET func_PacketsGet;
	WTPACKET func_Packet;

	void getExtraInfo(); // to help support new/untested tablets

	// tablet attributes
	bool hasPressure;
	float pressureScale;
	bool hasTilt;
	float azimuthScale;
	float altitudeScale;
//	float tiltScaleX;
//	float tiltScaleY;
//	UINT tiltMask;
	UINT cursorCount;
	UINT cursorBase;
	WTPKT allTools; // standard info available from any tool (mouse/pen/etc.)
	WTPKT someTools; // extra info available from only some tools

	// book-keeping
	std::map<GHOST_WindowWin32*,HCTX> contexts;
	HCTX contextForWindow(GHOST_WindowWin32*);

	std::vector<Tablet> tablets;

	GHOST_WindowWin32* activeWindow;
	TabletTool activeTool;
	BYTE sysButtonMap[32]; // user's custom button assignments for active tool

	int prevMouseX;
	int prevMouseY;
	UINT prevButtons;

	void convertTilt(ORIENTATION const&, TabletToolData&);
	bool convertButton(const UINT button, GHOST_TButtonMask&);

public:
	GHOST_TabletManagerWin32();
	~GHOST_TabletManagerWin32();

	bool available(); // another base class candidate

	void openForWindow(GHOST_WindowWin32*);
	void closeForWindow(GHOST_WindowWin32*);

	// returns whether any packets resulted in GHOST events
	bool processPackets(GHOST_WindowWin32* = NULL);
//	void processPacket(GHOST_WindowWin32*, UINT serialNumber);

	void changeTool(GHOST_WindowWin32*, UINT serialNumber);
	void dropTool();

	bool anyButtonsDown();
	};

#endif
