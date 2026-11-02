
#include "game.h"
#include "systemstub.h"

void Game::initMenu(int num) {
	_menuOption = -1;
	_menuHighlight = -1;
	if (_stub->hasBottomScreen()) {
		// 3DS: start on a safe entry (menu: first item, quit box: "No") and drop pending inputs
		_menuHighlight = (num == 2) ? 1 : 0;
		_stub->_pi.enter = false;
		_stub->_pi.tab = false;
		_stub->_pi.leftMouseButton = false;
		_stub->_touchPressed = false;
		_stub->_pi.dirMask = 0;
		_bottomHelp = true;
		drawBottomScreen();
	}
	_menuObjectCount = _sceneObjectsCount;
	_menuObjectMotion = _sceneObjectMotionsCount;
	_menuObjectFrames = _sceneObjectFramesCount;
	// the menu .mov is appended after the scene data; keep a copy of the slots it overwrites so the
	// scene comes back intact (a leftover "MENU" object made later scene loads fail on 3DS)
	memcpy(_menuSaveObjects, &_sceneObjectsTable[_menuObjectCount], sizeof(_menuSaveObjects));
	memcpy(_menuSaveMotions, &_sceneObjectMotionsTable[_menuObjectMotion], sizeof(_menuSaveMotions));
	memcpy(_menuSaveFrames, &_sceneObjectFramesTable[_menuObjectFrames], sizeof(_menuSaveFrames));
	memcpy(&_menuSaveAnimation, &_animationsTable[_animationsCount], sizeof(_menuSaveAnimation));
	const int animationsCount = _animationsCount;
	const int state = _loadDataState;
	char name[32];
	snprintf(name, sizeof(name), "..\\menu\\menu%d.wgp", num);
	loadWGP(name);
	snprintf(name, sizeof(name), "..\\menu\\menu%d.mov", num);
	loadMOV(name);
	_loadDataState = state;
	assert(_menuObjectMotion + 1 == _sceneObjectMotionsCount);
	assert(animationsCount + 1 == _animationsCount);
	_stub->setPalette(_bitmapBuffer0 + kOffsetBitmapPalette, 256);
	_stub->copyRectWidescreen(kGameScreenWidth, kGameScreenHeight, _bitmapBuffer1.bits, _bitmapBuffer1.pitch);
	_stub->showCursor(true);
}

void Game::finiMenu() {
	if (_currentSceneWgp[0]) {
		--_animationsCount;
		_sceneObjectsCount = _menuObjectCount;
		_sceneObjectMotionsCount = _menuObjectMotion;
		_sceneObjectFramesCount = _menuObjectFrames;
		memcpy(&_sceneObjectsTable[_menuObjectCount], _menuSaveObjects, sizeof(_menuSaveObjects));
		memcpy(&_sceneObjectMotionsTable[_menuObjectMotion], _menuSaveMotions, sizeof(_menuSaveMotions));
		memcpy(&_sceneObjectFramesTable[_menuObjectFrames], _menuSaveFrames, sizeof(_menuSaveFrames));
		memcpy(&_animationsTable[_animationsCount], &_menuSaveAnimation, sizeof(_menuSaveAnimation));
		const int state = _loadDataState;
		loadWGP(_currentSceneWgp);
		_loadDataState = state;
		_stub->setPalette(_bitmapBuffer0 + kOffsetBitmapPalette, 256);
		_stub->copyRectWidescreen(kGameScreenWidth, kGameScreenHeight, _bitmapBuffer1.bits, _bitmapBuffer1.pitch);
	}
	_stub->showCursor(false);
	if (_stub->hasBottomScreen()) {
		_bottomHelp = false;
		drawBottomScreen();
	}
}

void Game::handleMenu() {
	if (_menuHighlight != -1) {
		SceneObjectFrame *sof = &_sceneObjectFramesTable[_sceneObjectMotionsTable[_menuObjectMotion].firstFrameIndex + _menuHighlight];
		const int yPos = _bitmapBuffer1.h + 1 - sof->hdr.yPos - sof->hdr.h;
		copyBufferToBuffer(sof->hdr.xPos, yPos, sof->hdr.w, sof->hdr.h, &_bitmapBuffer3, &_bitmapBuffer1);
	}
	const int xCursor = _stub->_pi.mouseX;
	const int yCursor = _stub->_pi.mouseY;
	for (int frame = 0; frame < _sceneObjectMotionsTable[_menuObjectMotion].count; ++frame) {
		SceneObjectFrame *sof = &_sceneObjectFramesTable[_sceneObjectMotionsTable[_menuObjectMotion].firstFrameIndex + frame];
		// 3DS: no mouse; a stale position (left by a bag touch) would pin the highlight and block up/down
		if (!_stub->hasBottomScreen() && xCursor >= sof->hdr.xPos && xCursor < sof->hdr.xPos + sof->hdr.w && yCursor >= sof->hdr.yPos && yCursor < sof->hdr.yPos + sof->hdr.h) {
			_menuHighlight = frame;
			if (_stub->_pi.leftMouseButton) {
				_stub->_pi.leftMouseButton = false;
				_menuOption = frame;
			}
		}
	}

	if (_state == kStateMenu1) {
		// vertical layout
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			--_menuHighlight;
			if (_menuHighlight < 0) {
				_menuHighlight = 0;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			++_menuHighlight;
			if (_menuHighlight >= _sceneObjectMotionsTable[_menuObjectMotion].count) {
				_menuHighlight = _sceneObjectMotionsTable[_menuObjectMotion].count - 1;
			}
		}
	} else if (_state == kStateMenu2) {
		// horizontal layout
		if (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_LEFT;
			--_menuHighlight;
			if (_menuHighlight < 0) {
				_menuHighlight = 0;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
			++_menuHighlight;
			if (_menuHighlight >= _sceneObjectMotionsTable[_menuObjectMotion].count) {
				_menuHighlight = _sceneObjectMotionsTable[_menuObjectMotion].count - 1;
			}
		}
	}

	if (_stub->_pi.enter) {
		_stub->_pi.enter = false;
		_menuOption = _menuHighlight;
	}

	if (_menuHighlight != -1) {
		SceneObjectFrame *sof = &_sceneObjectFramesTable[_sceneObjectMotionsTable[_menuObjectMotion].firstFrameIndex + _menuHighlight];
		sof->decode(sof->data, _tempDecodeBuffer);
		const int yPos = _bitmapBuffer1.h + 1 - sof->hdr.yPos - sof->hdr.h;
		drawObject(sof->hdr.xPos, yPos, _tempDecodeBuffer, &_bitmapBuffer1);
	}

	_stub->copyRect(0, 0, kGameScreenWidth, kGameScreenHeight, _bitmapBuffer1.bits, _bitmapBuffer1.pitch);
}
