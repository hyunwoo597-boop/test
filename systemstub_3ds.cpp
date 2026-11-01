/*
 * Bermuda Syndrome engine rewrite
 * Nintendo 3DS backend (libctru, software rendering)
 *
 * Top screen    : the 640x480 game screen, three view modes (SELECT + X):
 *                 0 = 320x240 (whole screen, correct aspect)
 *                 1 = 400x240 (whole screen stretched to the full width)
 *                 2..4 = 1.25x / 1.67x / 2x zoom, scrolling with Jack
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
		memset(_panels, 0, sizeof(_panels));
		memset(_panelSplit, 0, sizeof(_panelSplit));
		_splitBuf = 0;
		_focusX = _focusY = -1;
		_zoomOffsetY = -1;
		loadViewMode();
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

	int _viewMode;
	int _focusX, _focusY;
	int _zoomOffsetX;
	int _zoomOffsetY; // 16.16 source row at the top of the screen in zoom mode (-1 = snap)

	void loadViewMode() {
		_viewMode = 0;
		FILE *fp = fopen("sdmc:/3ds/bermuda/view.cfg", "rb");
		if (fp) {
			const int c = fgetc(fp);
			if (c >= '0' && c <= '4') _viewMode = c - '0';
			fclose(fp);
		}
	}
	void saveViewMode() {
		FILE *fp = fopen("sdmc:/3ds/bermuda/view.cfg", "wb");
		if (fp) {
			fputc('0' + _viewMode, fp);
			fclose(fp);
		}
	}
	enum { kPanelMax = 3, kPanelAreaTop = 30, kPanelAreaBottom = 210 };
	struct Panel {
		uint32_t *rgb; // scaled RGB, 0xFF000000 = transparent
		int w, h, x, y, srcW, srcH, cropX, cropY;
		int split, splitH; // >0: source was cut at column split, right part stacked under the left one
	};
	Panel _panels[kPanelMax];

	int _panelSplit[kPanelMax];
	uint8_t *_splitBuf;
	virtual void setBottomPanelSplit(int id, int splitX) {
		if (id >= 0 && id < kPanelMax) _panelSplit[id] = splitX;
	}
	virtual void setBottomPanel(int id, const uint8_t *bits, int w, int h, int pitch) {
		if (id < 0 || id >= kPanelMax) return;
		Panel &pn = _panels[id];
		pn.split = 0;
		const int split = _panelSplit[id];
		if (bits && split > 0 && split < w) {
			// two rows: [0, split) on top, [split, w) below (bottom-up rows like the engine bitmaps)
			const int gap = 4;
			const int cw = MAX(split, w - split), chh = 2 * h + gap;
			_splitBuf = (uint8_t *)realloc(_splitBuf, cw * chh);
			if (_splitBuf) {
				memset(_splitBuf, 0, cw * chh);
				for (int y = 0; y < h; ++y) {
					const uint8_t *src = bits + (h - 1 - y) * pitch;
					memcpy(_splitBuf + (chh - 1 - y) * cw, src, split);
					memcpy(_splitBuf + (chh - 1 - (y + h + gap)) * cw, src + split, w - split);
				}
				setBottomPanelRaw(id, _splitBuf, cw, chh, cw);
				pn.split = split;
				pn.splitH = h + gap;
				return;
			}
		}
		setBottomPanelRaw(id, bits, w, h, pitch);
	}
	void setBottomPanelRaw(int id, const uint8_t *bits, int w, int h, int pitch) {
		Panel &pn = _panels[id];
		if (!bits) {
			if (pn.rgb) {
				free(pn.rgb);
				pn.rgb = 0;
				_bottomDirty = true;
			}
			return;
		}
		// crop to the opaque area, then fit into the bottom screen panel area (up to 2x)
		int cx0 = w, cy0 = h, cx1 = -1, cy1 = -1;
		for (int y = 0; y < h; ++y) {
			const uint8_t *row = bits + (h - 1 - y) * pitch;
			for (int x = 0; x < w; ++x) {
				if (row[x]) {
					if (x < cx0) cx0 = x;
					if (x > cx1) cx1 = x;
					if (y < cy0) cy0 = y;
					if (y > cy1) cy1 = y;
				}
			}
		}
		if (cx1 < 0) {
			setBottomPanel(id, 0, 0, 0, 0);
			return;
		}
		const int cw = cx1 - cx0 + 1, ch = cy1 - cy0 + 1;
		const bool full = (id == 1); // the bag window may use the whole bottom screen
		const bool portrait = (id == 2); // dialogue portrait: left column under the dialogue header
		const int areaTop = full ? 0 : (portrait ? 30 : kPanelAreaTop), areaBottom = full ? kBottomScreenH : (portrait ? kBottomScreenH - 4 : kPanelAreaBottom);
		const int areaW = portrait ? kBottomPortraitW - 6 : kBottomScreenW - (full ? 0 : 8), areaH = areaBottom - areaTop;
		int dw = cw * 2, dh = ch * 2;
		if (dw > areaW) { dh = dh * areaW / dw; dw = areaW; }
		if (dh > areaH) { dw = dw * areaH / dh; dh = areaH; }
		if (!pn.rgb || pn.w != dw || pn.h != dh) {
			free(pn.rgb);
			pn.rgb = (uint32_t *)malloc(dw * dh * sizeof(uint32_t));
			if (!pn.rgb) return;
		}
		pn.w = dw; pn.h = dh; pn.srcW = cw; pn.srcH = ch; pn.cropX = cx0; pn.cropY = cy0;
		pn.x = portrait ? 4 + (areaW - dw) / 2 : (kBottomScreenW - dw) / 2;
		pn.y = portrait ? areaTop : areaTop + (areaH - dh) / 2;
		// box filter: average the opaque source pixels covering each output pixel
		for (int y = 0; y < dh; ++y) {
			const int sy0 = cy0 + y * ch / dh, sy1 = MAX(sy0 + 1, cy0 + (y + 1) * ch / dh);
			for (int x = 0; x < dw; ++x) {
				const int sx0 = cx0 + x * cw / dw, sx1 = MAX(sx0 + 1, cx0 + (x + 1) * cw / dw);
				uint32_t r = 0, g = 0, b = 0, n = 0, t = 0;
				for (int sy = sy0; sy < sy1; ++sy) {
					const uint8_t *row = bits + (h - 1 - sy) * pitch; // bottom-up
					for (int sx = sx0; sx < sx1; ++sx) {
						const uint8_t c = row[sx];
						if (c == 0) { ++t; continue; }
						const uint32_t rgb = _pal[c];
						r += (rgb >> 16) & 0xFF; g += (rgb >> 8) & 0xFF; b += rgb & 0xFF; ++n;
					}
				}
				pn.rgb[y * dw + x] = (n == 0 || t > n) ? 0xFF000000 : (((r / n) << 16) | ((g / n) << 8) | (b / n));
			}
		}
		_bottomDirty = true;
	}
	virtual bool bottomPanelTouch(int id, int x, int y, int *px, int *py) {
		if (id < 0 || id >= kPanelMax || !_panels[id].rgb) return false;
		const Panel &pn = _panels[id];
		if (x < pn.x || y < pn.y || x >= pn.x + pn.w || y >= pn.y + pn.h) return false;
		*px = pn.cropX + (x - pn.x) * pn.srcW / pn.w;
		*py = pn.cropY + (y - pn.y) * pn.srcH / pn.h;
		if (pn.split > 0 && *py >= pn.splitH) {
			*px += pn.split;
			*py -= pn.splitH;
		}
		return true;
	}

	virtual void setFocus(int x, int y) {
		_focusX = x;
		_focusY = y;
	}

	// bilinear sample of the 640x480 buffer at 16.16 fixed point coordinates
	inline uint32_t sample(int fx, int fy) const {
		int x0 = fx >> 16, y0 = fy >> 16;
		if (x0 > _w - 2) x0 = _w - 2;
		if (y0 > _h - 2) y0 = _h - 2;
		const uint32_t ax = (fx >> 8) & 0xFF, ay = (fy >> 8) & 0xFF;
		const uint32_t *p = _gameBuffer + y0 * _w + x0;
		const uint32_t a = p[0], b = p[1], c = p[_w], d = p[_w + 1];
		const uint32_t w00 = (256 - ax) * (256 - ay), w10 = ax * (256 - ay), w01 = (256 - ax) * ay, w11 = ax * ay;
		const uint32_t r = (((a >> 16) & 0xFF) * w00 + ((b >> 16) & 0xFF) * w10 + ((c >> 16) & 0xFF) * w01 + ((d >> 16) & 0xFF) * w11) >> 16;
		const uint32_t g = (((a >> 8) & 0xFF) * w00 + ((b >> 8) & 0xFF) * w10 + ((c >> 8) & 0xFF) * w01 + ((d >> 8) & 0xFF) * w11) >> 16;
		const uint32_t bl = ((a & 0xFF) * w00 + (b & 0xFF) * w10 + (c & 0xFF) * w01 + (d & 0xFF) * w11) >> 16;
		return (r << 16) | (g << 8) | bl;
	}

	virtual void updateScreen() {
		if (_yuvMode) {
			return; // cutscene frames are presented from unlockYUV()
		}
		uint8_t *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
		if (_viewMode == 0) {
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
		} else {
			// mode 1: whole screen stretched to 400x240
			// mode 2..4: zoomed view (source step 1.6 / 1.2 / 1.0 pixel), scrolling with Jack
			static const int kZoomStep[] = { 0, 0, 0x19999 /* 1.6 */, 0x13333 /* 1.2 */, 0x10000 /* 1.0 */ };
			const int stepX = (_viewMode == 1) ? (_w << 16) / kTopW : kZoomStep[_viewMode];
			const int stepY = (_viewMode == 1) ? (_h << 16) / kTopH : stepX;
			int ox = 0, oy = 0;
			if (_viewMode >= 2) {
				const int maxX = (_w << 16) - kTopW * stepX;
				const int maxY = (_h << 16) - kTopH * stepY;
				int tx = maxX / 2, ty = maxY / 2;
				if (_focusX >= 0) tx = (_focusX << 16) - (kTopW / 2) * stepX;
				if (_focusY >= 0) ty = (_focusY << 16) - (kTopH / 2) * stepY;
				if (tx < 0) tx = 0;
				if (tx > maxX) tx = maxX;
				if (ty < 0) ty = 0;
				if (ty > maxY) ty = maxY;
				if (_zoomOffsetY < 0) {
					_zoomOffsetX = tx;
					_zoomOffsetY = ty;
				} else {
					_zoomOffsetX += (tx - _zoomOffsetX) / 6; // smooth follow
					_zoomOffsetY += (ty - _zoomOffsetY) / 6;
				}
				ox = _zoomOffsetX;
				oy = _zoomOffsetY;
			}
			for (int y = 0; y < kTopH; ++y) {
				int fy = oy + y * stepY + stepY / 2 - 0x8000;
				if (fy < 0) fy = 0;
				int fx = ox + stepX / 2 - 0x8000;
				for (int x = 0; x < kTopW; ++x, fx += stepX) {
					putPixel(fb, x, y, sample(fx < 0 ? 0 : fx, fy));
				}
			}
		}
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

		const bool select = (held & KEY_SELECT) != 0;

		setButton(down, up, KEY_A, _pi.enter);
		setButton(down, up, KEY_B, _pi.space);
		setButton(down, up, KEY_Y, _pi.shift);
		if (!select) setButton(down, up, KEY_X, _pi.tab);
		setButton(down, up, KEY_START, _pi.escape);
		if (select) {
			if (down & KEY_L) _pi.stateSlot = -1;
			if (down & KEY_R) _pi.stateSlot = 1;
			if (down & KEY_X) {
				_viewMode = (_viewMode + 1) % 5;
				_zoomOffsetY = -1;
				saveViewMode();
				clearTop();
			}
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
		bool panels = false;
		for (int i = 0; i < 2; ++i) panels |= _panels[i].rgb != 0; // the portrait does not hide the text
		for (int y = 0; y < kBottomScreenH; ++y) {
			const bool area = panels && ((y >= kPanelAreaTop && y < kPanelAreaBottom) || _panels[1].rgb); // panels replace the help text
			for (int x = 0; x < kBottomScreenW; ++x) {
				putPixel(fb, x, y, area ? kBottomPalette[0] : _bottom[y * kBottomScreenW + x]);
			}
		}
		for (int i = 0; i < kPanelMax; ++i) {
			const Panel &pn = _panels[i];
			if (!pn.rgb) continue;
			for (int y = 0; y < pn.h; ++y) {
				for (int x = 0; x < pn.w; ++x) {
					const uint32_t c = pn.rgb[y * pn.w + x];
					if (c != 0xFF000000) putPixel(fb, pn.x + x, pn.y + y, c);
				}
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
