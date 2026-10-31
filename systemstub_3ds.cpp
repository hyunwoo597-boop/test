/*
 * Bermuda Syndrome engine rewrite
 * Nintendo 3DS backend (libctru, software rendering)
 *
 * Top screen    : the 640x480 game screen box-filtered down to 320x240,
 *                 cutscenes (320x200 Cinepak) shown 1:1.
 * Bottom screen : controls help / save slot status, and during dialogues
 *                 the choices at native size (see Game::drawBottomDialogue).
 * Audio         : software mixer -> ndsp channel 0, 22050 Hz stereo.
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mixer.h"
#include "systemstub.h"
#include "util.h"

enum {
	kTopW = 400,
	kTopH = 240,
	kAudioRate = 22050,
	kAudioFramesPerBuf = 1024,
	kAudioBufCount = 3
};

static const uint32_t kBottomPalette[] = {
	0x101828, // kBottomColorBg
	0xFFF0A0, // kBottomColorSelected
	0xB8D8F8, // kBottomColorText
	0xFFB050, // kBottomColorTitle
	0x707888  // kBottomColorDim
};

// 3DS framebuffers are rotated: (x,y) -> ((x * 240) + (239 - y)) * 3, bytes B,G,R
static inline void putPixel(uint8_t *fb, int x, int y, uint32_t rgb) {
	uint8_t *p = fb + ((x * 240) + (239 - y)) * 3;
	p[0] = rgb & 0xFF;
	p[1] = (rgb >> 8) & 0xFF;
	p[2] = (rgb >> 16) & 0xFF;
}

struct SystemStub_3DS : SystemStub {

	uint32_t _pal[256];
	uint32_t *_gameBuffer; // 640x480 RGB888
	int _w, _h;
	Mixer *_mixer;

	// cutscene
	bool _yuvMode;
	int _yuvW, _yuvH;
	uint8_t *_yuvBuffer;

	// bottom screen (kept as RGB so it can be redrawn after a swap)
	uint32_t _bottom[kBottomScreenW * kBottomScreenH];
	bool _bottomDirty;

	// audio
	bool _ndspOk;
	volatile bool _audioRunning;
	Thread _audioThread;
	LightLock _audioLock;
	AudioCallback _audioProc;
	void *_audioParam;
	ndspWaveBuf _waveBuf[kAudioBufCount];
	int16_t *_audioMem;

	SystemStub_3DS()
		: _gameBuffer(0), _w(0), _h(0), _yuvMode(false), _yuvW(0), _yuvH(0), _yuvBuffer(0),
		  _bottomDirty(true), _ndspOk(false), _audioRunning(false), _audioThread(0),
		  _audioProc(0), _audioParam(0), _audioMem(0) {
		memset(_pal, 0, sizeof(_pal));
		memset(&_pi, 0, sizeof(_pi));
		for (int i = 0; i < kBottomScreenW * kBottomScreenH; ++i) {
			_bottom[i] = kBottomPalette[kBottomColorBg];
		}
		LightLock_Init(&_audioLock);
		_mixer = Mixer_Software_create(this);
	}
	virtual ~SystemStub_3DS() {
		delete _mixer;
	}

	virtual void init(const char *title, int w, int h, bool fullscreen, int screenMode) {
		_quit = false;
		_w = w;
		_h = h;
		_gameBuffer = (uint32_t *)calloc(w * h, sizeof(uint32_t));
		if (!_gameBuffer) {
			error("SystemStub_3DS::init() unable to allocate game buffer");
		}
		clearTop();
		initAudio();
		_mixer->open();
	}
	virtual void destroy() {
		_mixer->close();
		shutdownAudio();
		free(_gameBuffer);
		_gameBuffer = 0;
		free(_yuvBuffer);
		_yuvBuffer = 0;
	}

	virtual void setIcon(const uint8_t *data, int size) {}
	virtual void showCursor(bool show) {}

	virtual void setPalette(const uint8_t *pal, int n) {
		for (int i = 0; i < n; ++i) {
			_pal[i] = (pal[2] << 16) | (pal[1] << 8) | pal[0];
			pal += 4;
		}
	}
	virtual void fillRect(int x, int y, int w, int h, uint8_t color) {
		if (!clip(x, y, w, h)) return;
		uint32_t *dst = _gameBuffer + y * _w + x;
		const uint32_t rgb = _pal[color];
		for (int j = 0; j < h; ++j) {
			for (int i = 0; i < w; ++i) {
				dst[i] = rgb;
			}
			dst += _w;
		}
	}
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch, bool transparent) {
		if (x < 0 || y < 0 || x + w > _w || y + h > _h) return;
		uint32_t *dst = _gameBuffer + y * _w + x;
		buf += (h - 1) * pitch; // source bitmaps are bottom-up
		for (int j = 0; j < h; ++j) {
			for (int i = 0; i < w; ++i) {
				if (!transparent || buf[i] != 0) {
					dst[i] = _pal[buf[i]];
				}
			}
			dst += _w;
			buf -= pitch;
		}
	}
	virtual void darkenRect(int x, int y, int w, int h) {
		if (!clip(x, y, w, h)) return;
		uint32_t *dst = _gameBuffer + y * _w + x;
		for (int j = 0; j < h; ++j) {
			for (int i = 0; i < w; ++i) {
				dst[i] = (dst[i] >> 1) & 0x7F7F7F;
			}
			dst += _w;
		}
	}
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, int pitch) {}
	virtual void clearWidescreen() {}

	virtual void updateScreen() {
		if (_yuvMode) {
			return; // cutscene frames are presented from unlockYUV()
		}
		uint8_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
		// 2x2 box filter 640x480 -> 320x240, centered (40px side bars)
		for (int y = 0; y < kTopH; ++y) {
			const uint32_t *s0 = _gameBuffer + (y * 2) * _w;
			const uint32_t *s1 = s0 + _w;
			for (int x = 0; x < 320; ++x) {
				const uint32_t a = s0[x * 2], b = s0[x * 2 + 1], c = s1[x * 2], d = s1[x * 2 + 1];
				const uint32_t rb = (((a & 0xFF00FF) + (b & 0xFF00FF) + (c & 0xFF00FF) + (d & 0xFF00FF) + 0x020002) >> 2) & 0xFF00FF;
				const uint32_t g  = (((a & 0x00FF00) + (b & 0x00FF00) + (c & 0x00FF00) + (d & 0x00FF00) + 0x000200) >> 2) & 0x00FF00;
				putPixel(fb, 40 + x, y, rb | g);
			}
		}
		clearSideBars(fb);
		presentBottom();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	virtual void setYUV(bool flag, int w, int h) {
		_yuvMode = flag;
		if (flag) {
			_yuvW = w;
			_yuvH = h;
			free(_yuvBuffer);
			_yuvBuffer = (uint8_t *)calloc(w * h, 2);
			clearTop();
		} else {
			free(_yuvBuffer);
			_yuvBuffer = 0;
			clearTop();
		}
	}
	virtual uint8_t *lockYUV(int *pitch) {
		*pitch = _yuvW * 2;
		return _yuvBuffer;
	}
	virtual void unlockYUV() {
		if (!_yuvBuffer) return;
		uint8_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
		const int ox = (kTopW - _yuvW) / 2;
		const int oy = (kTopH - _yuvH) / 2;
		for (int y = 0; y < _yuvH && y + oy < kTopH; ++y) {
			const uint8_t *p = _yuvBuffer + y * _yuvW * 2;
			for (int x = 0; x < _yuvW; x += 2, p += 4) { // UYVY
				const int u = p[0] - 128, y0 = p[1], v = p[2] - 128, y1 = p[3];
				putPixel(fb, ox + x,     oy + y, yuvToRgb(y0, u, v));
				putPixel(fb, ox + x + 1, oy + y, yuvToRgb(y1, u, v));
			}
		}
		presentBottom();
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	virtual void processEvents() {
		if (!aptMainLoop()) {
			_quit = true;
			return;
		}
		hidScanInput();
		const u32 down = hidKeysDown();
		const u32 up = hidKeysUp();
		const u32 held = hidKeysHeld();

		if ((held & KEY_START) && (held & KEY_SELECT)) {
			_quit = true;
			return;
		}
		const bool select = (held & KEY_SELECT) != 0;

		setButton(down, up, KEY_A, _pi.enter);
		setButton(down, up, KEY_B, _pi.space);
		setButton(down, up, KEY_Y, _pi.shift);
		setButton(down, up, KEY_X, _pi.tab);
		setButton(down, up, KEY_START, _pi.escape);
		if (select) {
			if (down & KEY_L) _pi.stateSlot = -1;
			if (down & KEY_R) _pi.stateSlot = 1;
		} else {
			setButton(down, up, KEY_L, _pi.ctrl);
			if (down & KEY_R) _pi.shift = true;
			if ((up & KEY_R) && !(held & KEY_Y)) _pi.shift = false;
		}
		if (down & KEY_ZR) _pi.save = true;
		if (down & KEY_ZL) _pi.load = true;

		setDir(down, up, KEY_DUP | KEY_CPAD_UP, PlayerInput::DIR_UP);
		setDir(down, up, KEY_DDOWN | KEY_CPAD_DOWN, PlayerInput::DIR_DOWN);
		setDir(down, up, KEY_DLEFT | KEY_CPAD_LEFT, PlayerInput::DIR_LEFT);
		setDir(down, up, KEY_DRIGHT | KEY_CPAD_RIGHT, PlayerInput::DIR_RIGHT);

		if (down & KEY_TOUCH) {
			touchPosition tp;
			hidTouchRead(&tp);
			_touchPressed = true;
			_touchX = tp.px;
			_touchY = tp.py;
		}
	}
	virtual void sleep(int duration) {
		if (duration > 0) {
			svcSleepThread((s64)duration * 1000000LL);
		}
	}
	virtual uint32_t getTimeStamp() {
		return (uint32_t)osGetTime();
	}

	virtual void lockAudio() { LightLock_Lock(&_audioLock); }
	virtual void unlockAudio() { LightLock_Unlock(&_audioLock); }
	virtual void startAudio(AudioCallback callback, void *param) {
		LightLock_Lock(&_audioLock);
		_audioProc = callback;
		_audioParam = param;
		LightLock_Unlock(&_audioLock);
	}
	virtual void stopAudio() {
		LightLock_Lock(&_audioLock);
		_audioProc = 0;
		_audioParam = 0;
		LightLock_Unlock(&_audioLock);
	}
	virtual int getOutputSampleRate() { return kAudioRate; }
	virtual Mixer *getMixer() { return _mixer; }

	virtual bool hasBottomScreen() const { return true; }
	virtual void copyBottomScreen(const uint8_t *buf, int pitch) {
		const uint8_t *src = buf + (kBottomScreenH - 1) * pitch; // bottom-up
		for (int y = 0; y < kBottomScreenH; ++y) {
			for (int x = 0; x < kBottomScreenW; ++x) {
				const uint8_t c = src[x];
				_bottom[y * kBottomScreenW + x] = kBottomPalette[c < 5 ? c : 0];
			}
			src -= pitch;
		}
		_bottomDirty = true;
	}

	// helpers

	bool clip(int &x, int &y, int &w, int &h) const {
		if (x < 0) { w += x; x = 0; }
		if (y < 0) { h += y; y = 0; }
		if (x + w > _w) w = _w - x;
		if (y + h > _h) h = _h - y;
		return w > 0 && h > 0;
	}
	static void setButton(u32 down, u32 up, u32 key, bool &state) {
		if (down & key) state = true;
		if (up & key) state = false;
	}
	void setDir(u32 down, u32 up, u32 keys, int mask) {
		if (down & keys) _pi.dirMask |= mask;
		if (up & keys) {
			const u32 held = hidKeysHeld();
			if (!(held & keys)) _pi.dirMask &= ~mask;
		}
	}
	static uint32_t yuvToRgb(int y, int u, int v) {
		int r = y + ((91881 * v) >> 16);
		int g = y - ((22554 * u + 46802 * v) >> 16);
		int b = y + ((116130 * u) >> 16);
		r = r < 0 ? 0 : (r > 255 ? 255 : r);
		g = g < 0 ? 0 : (g > 255 ? 255 : g);
		b = b < 0 ? 0 : (b > 255 ? 255 : b);
		return (r << 16) | (g << 8) | b;
	}
	void clearSideBars(uint8_t *fb) {
		memset(fb, 0, 40 * 240 * 3);
		memset(fb + 360 * 240 * 3, 0, 40 * 240 * 3);
	}
	void clearTop() {
		for (int i = 0; i < 2; ++i) {
			uint8_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
			memset(fb, 0, kTopW * kTopH * 3);
			presentBottom();
			gfxFlushBuffers();
			gfxSwapBuffers();
		}
	}
	void presentBottom() {
		// the bottom screen is single-buffered: only rewrite it when it changed
		if (!_bottomDirty) return;
		uint8_t *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, 0, 0);
		for (int y = 0; y < kBottomScreenH; ++y) {
			for (int x = 0; x < kBottomScreenW; ++x) {
				putPixel(fb, x, y, _bottom[y * kBottomScreenW + x]);
			}
		}
		_bottomDirty = false;
	}

	// audio

	void initAudio() {
		_ndspOk = R_SUCCEEDED(ndspInit());
		if (!_ndspOk) {
			warning("ndspInit failed (dump the DSP firmware to sdmc:/3ds/dspfirm.cdc), audio disabled");
			return;
		}
		ndspSetOutputMode(NDSP_OUTPUT_STEREO);
		ndspChnReset(0);
		ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
		ndspChnSetRate(0, kAudioRate);
		ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
		_audioMem = (int16_t *)linearAlloc(kAudioBufCount * kAudioFramesPerBuf * 2 * sizeof(int16_t));
		if (!_audioMem) {
			_ndspOk = false;
			ndspExit();
			return;
		}
		memset(_audioMem, 0, kAudioBufCount * kAudioFramesPerBuf * 2 * sizeof(int16_t));
		memset(_waveBuf, 0, sizeof(_waveBuf));
		for (int i = 0; i < kAudioBufCount; ++i) {
			_waveBuf[i].data_vaddr = _audioMem + i * kAudioFramesPerBuf * 2;
			_waveBuf[i].nsamples = kAudioFramesPerBuf;
			_waveBuf[i].status = NDSP_WBUF_DONE;
		}
		s32 prio = 0x30;
		svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
		_audioRunning = true;
		_audioThread = threadCreate(audioThreadEntry, this, 32 * 1024, prio - 1, -2, false);
		if (!_audioThread) {
			_audioRunning = false;
		}
	}
	void shutdownAudio() {
		if (_audioThread) {
			_audioRunning = false;
			threadJoin(_audioThread, U64_MAX);
			threadFree(_audioThread);
			_audioThread = 0;
		}
		if (_ndspOk) {
			ndspChnReset(0);
			ndspExit();
			_ndspOk = false;
		}
		if (_audioMem) {
			linearFree(_audioMem);
			_audioMem = 0;
		}
	}
	static void audioThreadEntry(void *arg) {
		((SystemStub_3DS *)arg)->audioLoop();
	}
	void audioLoop() {
		while (_audioRunning) {
			for (int i = 0; i < kAudioBufCount; ++i) {
				ndspWaveBuf *wb = &_waveBuf[i];
				if (wb->status == NDSP_WBUF_DONE || wb->status == NDSP_WBUF_FREE) {
					int16_t *samples = (int16_t *)wb->data_vaddr;
					const int bytes = kAudioFramesPerBuf * 2 * sizeof(int16_t);
					LightLock_Lock(&_audioLock);
					if (_audioProc) {
						_audioProc(_audioParam, (uint8_t *)samples, bytes);
					} else {
						memset(samples, 0, bytes);
					}
					LightLock_Unlock(&_audioLock);
					DSP_FlushDataCache(samples, bytes);
					ndspChnWaveBufAdd(0, wb);
				}
			}
			svcSleepThread(4 * 1000000LL);
		}
	}
};

SystemStub *SystemStub_3DS_create() {
	return new SystemStub_3DS();
}

// Called by error(): show the message on the bottom screen and wait for START,
// so a missing data file is visible instead of silently returning to the menu.
void platformFatalError(const char *msg) {
	consoleInit(GFX_BOTTOM, NULL);
	printf("\x1b[2J\x1b[1;1HBermuda Syndrome - fatal error\n\n%s\n\n", msg);
	printf("Data must be in sdmc:/3ds/bermuda/BERMUDA/\n\nPress START to exit.\n");
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) {
			break;
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	gfxExit();
}

// default 3dsx main thread stack (32 KB) is tight for the recursive data scan
extern "C" {
u32 __stacksize__ = 256 * 1024;
}
