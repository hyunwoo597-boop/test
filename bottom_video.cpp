/*
 * 3DS: looping video on the bottom screen while the status bar is hidden (L button).
 * bottom.vid: 'BVD1', u16 w, h, fps, count, 4 pad, u32 offsets[count + 1], baseline JPEG frames.
 * Looked up in the working directory (sdmc:/3ds/bermuda) first, then romfs:/.
 */

#include "game.h"
#include "file.h"
#include "systemstub.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#include "stb_image.h"

static uint8_t *_bvData;
static uint32_t _bvSize;
static bool _bvTried;
static int _bvW, _bvH, _bvFps, _bvCount, _bvFrame = -1;
static bool _bvShown;

static bool loadBottomVideo() {
	if (_bvTried) {
		return _bvData != 0;
	}
	_bvTried = true;
	static const char *paths[] = { "bottom.vid", "romfs:/bottom.vid", 0 };
	for (int i = 0; paths[i]; ++i) {
		File f;
		if (!f.open(paths[i])) {
			continue;
		}
		_bvSize = f.size();
		_bvData = (uint8_t *)malloc(_bvSize);
		if (!_bvData) {
			return false;
		}
		f.read(_bvData, _bvSize);
		if (_bvSize < 16 || memcmp(_bvData, "BVD1", 4) != 0) {
			free(_bvData);
			_bvData = 0;
			continue;
		}
		_bvW = READ_LE_UINT16(_bvData + 4);
		_bvH = READ_LE_UINT16(_bvData + 6);
		_bvFps = READ_LE_UINT16(_bvData + 8);
		_bvCount = READ_LE_UINT16(_bvData + 10);
		if (_bvFps <= 0 || _bvCount <= 0 || 16 + 4 * (_bvCount + 1) > (int)_bvSize) {
			free(_bvData);
			_bvData = 0;
			continue;
		}
		return true;
	}
	return false;
}

// the startup logo movies, until the first frame of the main loop
static bool _bottomIntro = true;

void Game::updateBottomVideo() {
	// bottom.vid: the startup logos and the title screen
	const bool show = _stub->hasBottomScreen() && (_bottomIntro || _state == kStateBitmap);
	if (!show) {
		if (_bvShown) {
			_stub->setBottomVideo(0, 0, 0);
			_bvShown = false;
			_bvFrame = -1;
		}
		return;
	}
	if (!loadBottomVideo()) {
		return;
	}
	const int frame = (_stub->getTimeStamp() * _bvFps / 1000) % _bvCount;
	if (frame == _bvFrame && _bvShown) {
		return;
	}
	const uint32_t o0 = READ_LE_UINT32(_bvData + 16 + 4 * frame), o1 = READ_LE_UINT32(_bvData + 16 + 4 * (frame + 1));
	if (o1 <= o0 || o1 > _bvSize) {
		return;
	}
	int w, h, n;
	uint8_t *rgb = stbi_load_from_memory(_bvData + o0, o1 - o0, &w, &h, &n, 3);
	if (rgb) {
		_stub->setBottomVideo(rgb, w, h);
		stbi_image_free(rgb);
		_bvShown = true;
		_bvFrame = frame;
	}
}

// 3DS: a picture (bottom_splash.jpg, 320x240) on the bottom screen during the opening scenes
// The menus keep the bottom screen for their own panels.
static bool _bsShown;

static bool loadBottomSplashFile(uint8_t **rgb, int *w, int *h) {
	static const char *paths[] = { "bottom_splash.jpg", "romfs:/bottom_splash.jpg", 0 };
	for (int i = 0; paths[i]; ++i) {
		File f;
		if (!f.open(paths[i])) {
			continue;
		}
		const uint32_t size = f.size();
		uint8_t *data = (uint8_t *)malloc(size);
		if (!data) {
			return false;
		}
		f.read(data, size);
		int n;
		*rgb = stbi_load_from_memory(data, size, w, h, &n, 3);
		free(data);
		if (*rgb && *w == 320 && *h == 240) {
			return true;
		}
		if (*rgb) {
			stbi_image_free(*rgb);
		}
		*rgb = 0;
	}
	return false;
}

void Game::updateBottomSplash() {
	// bottom_splash.jpg: the opening scenes, and the gameplay when the status bar is hidden (L)
	const bool show = _stub->hasBottomScreen() && _state == kStateGame && (_sceneNumber <= -1000 || !_lifeBarDisplayed);
	_bottomIntro = false;
	if (show == _bsShown) {
		return;
	}
	_bsShown = show;
	if (!show) {
		_stub->setBottomSplash(0, 0, 0);
		return;
	}
	_stub->setBottomPanel(0, 0, 0, 0, 0);
	_stub->setBottomPanel(1, 0, 0, 0, 0);
	uint8_t *rgb = 0;
	int w = 0, h = 0;
	if (loadBottomSplashFile(&rgb, &w, &h)) {
		_stub->setBottomSplash(rgb, w, h);
		stbi_image_free(rgb);
	}
}
