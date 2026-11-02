/*
 * Bermuda Syndrome engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir
 */

#ifndef SYSTEMSTUB_H__
#define SYSTEMSTUB_H__

#include "intern.h"

struct Mixer;

struct PlayerInput {
	enum {
		DIR_UP    = 1 << 0,
		DIR_DOWN  = 1 << 1,
		DIR_LEFT  = 1 << 2,
		DIR_RIGHT = 1 << 3
	};
	uint8_t dirMask;
	bool enter; // use, current action
	bool space; // take gun, shoot
	bool shift; // put back gun, run
	bool ctrl; // toggle status window
	bool tab; // toggle bag window
	bool escape; // quit
	bool leftMouseButton, rightMouseButton;
	int mouseX, mouseY;
	bool save;
	bool load;
	int stateSlot;
	bool fastMode;
};

enum {
	kBottomScreenW = 320,
	kBottomScreenH = 240,
	kBottomPortraitW = 96, // dialogue portrait column on the bottom screen (3DS)
	kBottomColorBg = 0,
	kBottomColorSelected = 1,
	kBottomColorText = 2,
	kBottomColorTitle = 3,
	kBottomColorDim = 4
};

enum {
	SCREEN_MODE_DEFAULT,
	SCREEN_MODE_4_3,
	SCREEN_MODE_16_9
};

struct SystemStub {
	typedef void (*AudioCallback)(void *param, uint8_t *buf, int len);

	bool _quit;
	PlayerInput _pi;

	// second screen / touch (used by the 3DS port, no-op elsewhere)
	bool _touchPressed;
	int _touchX, _touchY;

	SystemStub() : _quit(false), _touchPressed(false), _touchX(0), _touchY(0) {}
	virtual ~SystemStub() {}

	// buf: 320x240 bottom-up 8bpp, values are kBottomColor* indices
	virtual void copyBottomScreen(const uint8_t *buf, int pitch) {}
	virtual bool hasBottomScreen() const { return false; }
	virtual void setFocus(int x, int y) {}
	// bottom-screen panels (3DS): id 0 = status bar, 1 = bag window, 2 = dialogue portrait; bits = palette indices, bottom-up rows, 0 = transparent; bits 0 hides
	virtual void setBottomPanel(int id, const uint8_t *bits, int w, int h, int pitch) {}
	virtual void setBottomPanelSplit(int id, int splitX) {}
	virtual void setBottomVideo(const uint8_t *rgb, int w, int h) {} // 3DS: video frame (RGB24, top-down) in the middle of the bottom screen, 0 = off
	virtual void addStereoObject(int x, int y, int w, int h) {} // 3DS: rectangle (top-down) of a scene object drawn in the next full frame, its bottom edge sets its depth
	virtual void setStereoBackground(const uint8_t *buf, int pitch) {} // 3DS: scene background of the next full frame (bottom-up), objects are drawn in front of it
	virtual void setBagButton(bool visible) {} // 3DS: bag button in the top-right corner of the bottom screen
	virtual bool bottomPanelTouch(int id, int x, int y, int *px, int *py) { return false; } // player position on the 640x480 screen (-1 = none), used by zoomed views

	virtual void init(const char *title, int w, int h, bool fullscreen, int screenMode) = 0;
	virtual void destroy() = 0;

	virtual void setIcon(const uint8_t *data, int size) = 0;
	virtual void showCursor(bool show) = 0;

	virtual void setPalette(const uint8_t *pal, int n) = 0;
	virtual void fillRect(int x, int y, int w, int h, uint8_t color) = 0;
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch, bool transparent = false) = 0;
	virtual void darkenRect(int x, int y, int w, int h) = 0;
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, int pitch) = 0;
	virtual void clearWidescreen() = 0;
	virtual void updateScreen() = 0;

	virtual void setYUV(bool flag, int w, int h) = 0;
	virtual uint8_t *lockYUV(int *pitch) = 0;
	virtual void unlockYUV() = 0;

	virtual void processEvents() = 0;
	virtual void sleep(int duration) = 0;
	virtual uint32_t getTimeStamp() = 0;

	virtual void lockAudio() = 0;
	virtual void unlockAudio() = 0;
	virtual void startAudio(AudioCallback callback, void *param) = 0;
	virtual void stopAudio() = 0;
	virtual int getOutputSampleRate() = 0;

	virtual Mixer *getMixer() = 0;
};

extern SystemStub *SystemStub_SDL_create();
extern SystemStub *SystemStub_3DS_create();

#endif // SYSTEMSTUB_H__
