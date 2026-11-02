/*
 * Bermuda Syndrome engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir
 */

#include "decoder.h"
#include "file.h"
#include "game.h"
#include "str.h"
#include "systemstub.h"
#include "bottom_strings_3ds.h"

static const Rect _dialogueBackgroundRect = { 33, 165, 574, 150 };
static const int _selectedDialogueChoiceColor = 0xCAF6FF;
static const int _unselectedDialogueChoiceColor = 0x6DC6FA;

static void drawChar(uint8_t *dst, int dstPitch, const uint16_t *fontData, int c, uint8_t color) {
	int offset = c * 16;
	for (int i = 0; i < 16; ++i) {
		uint16_t chr = fontData[offset + i];
		for (int b = 0; b < 16; ++b) {
			if (chr & (1 << b)) {
				dst[-i * dstPitch + b] = color;
			}
		}
	}
}

static int getHangulGlyphOffset(int codeLo, int codeHi) {
	int offset, num;
	if (codeHi == 0x20) {
		return codeLo * 7;
	} else if (codeLo <= 0xAC) {
		offset = 128 * 7;
		num = codeLo - 0xA1;
	} else if (codeLo <= 0xC8) {
		offset = 1256 * 7;
		num = codeLo - 0xB0;
	} else {
		offset = 3606 * 7;
		num = codeLo - 0xCA;
	}
	return offset + (num * 94 + (codeHi - 0xA1)) * 7;
}

static void drawHangulGlyph(uint8_t *dst, int dstPitch, const uint8_t *fontData, uint32_t fontLutOffset, int c, uint8_t color) {
	if (!fontData) return;
	int offset = getHangulGlyphOffset(c >> 8, c & 0xFF);

	const uint8_t *p = fontData + fontLutOffset + offset;
	const int bufferIndex = READ_LE_UINT16(p);
	const int bufferOffset = READ_LE_UINT16(p + 2);

	p = fontData + 38 + bufferIndex * 16;
	offset = READ_LE_UINT32(p);
	assert(bufferOffset < READ_LE_UINT16(p + 4));
	p = fontData + offset + bufferOffset;

	for (int y = 0; y < 16; ++y, p += 2) {
		const int bits = p[0] * 256 + p[1];
		for (int x = 0; x < 16; ++x) {
			if ((bits & (1 << (15 - x))) == 0) {
				dst[-y * dstPitch + x] = color;
			}
		}
	}
}

static uint8_t findBestMatchingColor(const uint8_t *src, int color) {
	uint8_t bestColor = 0;
	int bestSum = -1;
	int r =  color        & 0xFF;
	int g = (color >>  8) & 0xFF;
	int b = (color >> 16) & 0xFF;
	for (int i = 0; i < 256; ++i) {
		int dr = r - src[2];
		int dg = g - src[1];
		int db = b - src[0];
		int sum = dr * dr + dg * dg + db * db;
		if (bestSum == -1 || sum < bestSum) {
			bestSum = sum;
			bestColor = i;
		}
		src += 4;
	}
	return bestColor;
}

void Game::redrawDialogueTexts() {
	debug(DBG_DIALOGUE, "Game::redrawDialogueTexts()");
	char *substringOffset[NUM_DIALOG_CHOICES][8];
	for (int i = 0; i < _dialogueChoiceCounter; ++i) {
		char *lastWord = 0;
		int lastStringLen = 0;
		int stringLen = 0;
		int substringCount = 0;
		for (char *p = _dialogueChoiceText[i]; *p; ++p) {
			int chr = (uint8_t)*p;
			if (_textCp949 && (chr & 0x80) != 0) {
				++p;
				stringLen += 16;
			} else {
				stringLen += _fontCharWidth[chr] + 1;
			}
			if (stringLen > _dialogueTextRect.w) {
				assert(substringCount < 8);
				substringOffset[i][substringCount] = lastWord;
				++substringCount;
				stringLen -= lastStringLen;
			}
			if (chr == ' ') {
				lastWord = p;
				lastStringLen = stringLen;
			}
		}
	}

	uint8_t *textBuffer = (uint8_t *)malloc(_dialogueTextRect.w * _dialogueTextRect.h);
	if (!textBuffer) {
		return;
	}

	memset(textBuffer, 0, _dialogueTextRect.w * _dialogueTextRect.h);
	int y = 0;
	for (int i = 0; i < _dialogueChoiceCounter; ++i) {
		int color = (_dialogueSpeechIndex == i) ? _selectedDialogueChoiceColor : _unselectedDialogueChoiceColor;
		uint8_t choiceColor = findBestMatchingColor(_bitmapBuffer0 + kOffsetBitmapPalette, color);
		int x = 0;
		int substring = 0;
		for (char *p = _dialogueChoiceText[i]; *p; ++p) {
			if (p == substringOffset[i][substring]) {
				++substring;
				y += 16;
				x = 0;
			} else {
				if (y + 16 >= _dialogueTextRect.h) {
					break;
				}
				int chr = (uint8_t)*p;
				if (_textCp949 && (chr & 0x80) != 0) {
					++p;
					chr = (chr << 8) | (uint8_t)*p;
					drawHangulGlyph(textBuffer + (_dialogueTextRect.h - 1 - y) * _dialogueTextRect.w + x, _dialogueTextRect.w, _hangulFontData, _hangulFontLutOffset, chr, choiceColor);
					x += 16;
				} else {
					drawChar(textBuffer + (_dialogueTextRect.h - 1 - y) * _dialogueTextRect.w + x, _dialogueTextRect.w, _fontData, chr, choiceColor);
					x += _fontCharWidth[chr] + 1;
				}
			}
		}
		y += 16;
	}
	if (!_stub->hasBottomScreen()) {
		_stub->copyRect(_dialogueTextRect.x, _dialogueTextRect.y, _dialogueTextRect.w, _dialogueTextRect.h, textBuffer, _dialogueTextRect.w, true);
	}
	free(textBuffer);
	if (_stub->hasBottomScreen()) {
		drawBottomDialogue();
	}
}

void Game::initDialogue() {
	playMusic("..\\midi\\sadialog.mid");
	for (int spr = 0; spr < 3; ++spr) {
		for (int i = 0; i < 105; ++i) {
			_dialogueSpriteDataTable[spr][i] = 0;
		}
	}

	_dialogueFrameSpriteData = loadFile("..\\wgp\\frame.spr");
	loadDialogueSprite(0);
	loadDialogueSprite(1);
	loadDialogueSprite(2);
	loadDialogueData(_scriptDialogFileName);

	_stub->_pi.dirMask = 0;
	_stub->_pi.escape = false;
	_stub->_pi.enter = false;
}

void Game::handleDialogue() {
	debug(DBG_DIALOGUE, "Game::handleDialogue()");
		if (_stub->hasBottomScreen()) {
			handleBottomTouch();
		}

		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			if (_dialogueChoiceSelected == 0 && _dialogueSpeechIndex < _dialogueChoiceCounter - 1) {
				++_dialogueSpeechIndex;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			if (_dialogueChoiceSelected == 0 && _dialogueSpeechIndex > 0) {
				--_dialogueSpeechIndex;
			}
		}
		if (_stub->_pi.escape) {
			_stub->_pi.escape = false;
			_nextState = kStateGame;
			return;
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			if (_dialogueChoiceCounter > 1 && _dialogueChoiceSelected == 0) {
				win16_sndPlaySound(3, _dialogueChoiceSpeechSoundFile[_dialogueSpeechIndex]);
				_dialogueChoiceSelected = 1;
			} else {
				win16_sndPlaySound(6);
			}
		}
		if (_dialogueChoiceSelected != 0 && win16_sndPlaySound(22)) {
			_dialogueChoiceSelected = 0;
			strcpy(_tempTextBuffer, _dialogueChoiceNextId[_dialogueSpeechIndex]);
			if (_dialogueChoiceGotoFlag[_dialogueSpeechIndex]) {
				setupDialog(_dialogueChoiceNextId[_dialogueSpeechIndex]);
				if (_dialogueChoiceCounter == 0) {
					_nextState = kStateGame;
					return;
				}
			} else {
				int n = atoi(_tempTextBuffer);
				if (n == 100) { // special scene, Jack kisses Natalia
					_dialogueSpriteIndex = 2;
				} else {
					_dialogueEndedFlag = 1;
					_lastDialogueEndedId = atoi(_tempTextBuffer);
					return;
				}
			}
		}

		redrawDialogueBackground();
		redrawDialogueSprite(_dialogueSpriteIndex);
		if (_dialogueSpriteIndex == 2) {
			if (_dialogueSpriteCurrentFrameTable[2] == 0) {
				_dialogueEndedFlag = 1;
				return;
			}
		} else {
			redrawDialogueTexts();
		}
}

void Game::finiDialogue() {
	if (_stub->hasBottomScreen()) {
		_stub->setBottomPanel(2, 0, 0, 0, 0);
		drawBottomScreen();
	}
	unloadDialogueData();
	if (_dialogueFrameSpriteData) {
		free(_dialogueFrameSpriteData);
		_dialogueFrameSpriteData = 0;
	}
	for (int spr = 0; spr < 3; ++spr) {
		for (int i = 0; i < 105; ++i) {
			free(_dialogueSpriteDataTable[spr][i]);
			_dialogueSpriteDataTable[spr][i] = 0;
		}
	}
	playMusic(_musicName);
	memset(_keysPressed, 0, sizeof(_keysPressed));
}

void Game::unloadDialogueData() {
	debug(DBG_DIALOGUE, "Game::unloadDialogueData() %d", _loadDialogueDataState);
	if (_loadDialogueDataState == 2) {
		free(_dialogueDescriptionBuffer);
		_dialogueDescriptionBuffer = 0;
		_loadDialogueDataState = 1;
	}
}

void Game::setupDialog(const char *dialogId) {
	debug(DBG_DIALOGUE, "Game::setupDialog('%s')", dialogId);
	if (dialogId[0] == 'J' || dialogId[0] == 'j') {
		_dialogueSpriteIndex = 0;
	} else {
		_dialogueSpriteIndex = 1;
	}
	_dialogueSpeechIndex = 0;
	_dialogueChoiceCounter = 0;
	for (int i = 0; i < _dialogueChoiceSize; ++i) {
		if (strcmp(_dialogueChoiceData[i].id, dialogId) == 0) {
			assert(_dialogueChoiceCounter < NUM_DIALOG_CHOICES);
			_dialogueChoiceGotoFlag[_dialogueChoiceCounter] = _dialogueChoiceData[i].gotoFlag;
			_dialogueChoiceNextId[_dialogueChoiceCounter] = _dialogueChoiceData[i].nextId;
			_dialogueChoiceSpeechSoundFile[_dialogueChoiceCounter] = _dialogueChoiceData[i].speechSoundFile;
			_dialogueChoiceText[_dialogueChoiceCounter] = _dialogueChoiceData[i].text;
			debug(DBG_DIALOGUE, "Add dialog choice goto %d nextId %s speechSoundFile %s text \"%s\"",
				_dialogueChoiceGotoFlag[_dialogueChoiceCounter],
				_dialogueChoiceNextId[_dialogueChoiceCounter],
				_dialogueChoiceSpeechSoundFile[_dialogueChoiceCounter],
				_dialogueChoiceText[_dialogueChoiceCounter]);
			++_dialogueChoiceCounter;
		}
	}
	// only one choice, play the speech directly
	if (_dialogueChoiceCounter == 1) {
		win16_sndPlaySound(3, _dialogueChoiceSpeechSoundFile[_dialogueSpeechIndex]);
		_dialogueChoiceSelected = 1;
	}
}

void Game::loadDialogueSprite(int spr) {
	debug(DBG_DIALOGUE, "Game::loadDialogueSprite(%d)", spr);
	const char *spriteFile = "..\\wgp\\kiss.spr";
	switch (spr) {
	case 0:
		spriteFile = _scriptDialogSprite1;
		break;
	case 1:
		spriteFile = _scriptDialogSprite2;
		break;
	}
	FileHolder fp(_fs, spriteFile);
	int tag = fp->readUint16LE();
	if (tag != 0x3553) {
		error("Invalid spr format %X", tag);
	}
	int count = fp->readUint16LE();
	assert(count <= 105);
	for (int i = 0; i < count; ++i) {
		int size = fp->readUint16LE();
		_dialogueSpriteDataTable[spr][i] = (uint8_t *)malloc(size + 10);
		if (_dialogueSpriteDataTable[spr][i]) {
			fp->read(_dialogueSpriteDataTable[spr][i], size + 10);
		}
	}
	_dialogueSpriteFrameCountTable[spr] = count;
	_dialogueSpriteCurrentFrameTable[spr] = 0;
}

void Game::loadDialogueData(const char *filename) {
	debug(DBG_DIALOGUE, "Game::loadDialogueData(%s)", filename);
	if (_loadDialogueDataState == 0) {
		_loadDialogueDataState = 1;
	} else {
		unloadDialogueData();
	}
	FileHolder fp(_fs, filename);
	_dialogueDescriptionSize = fp->size();
	_dialogueDescriptionBuffer = (char *)malloc(_dialogueDescriptionSize + 1);
	if (_dialogueDescriptionBuffer) {
		_loadDialogueDataState = 2;
		fp->read(_dialogueDescriptionBuffer, _dialogueDescriptionSize);
		_dialogueDescriptionBuffer[_dialogueDescriptionSize] = 0;
		stringStripComments(_dialogueDescriptionBuffer);
		parseDLG();
		setupDialog(_scriptDialogId);
	}
}

void Game::redrawDialogueSprite(int num) {
	debug(DBG_DIALOGUE, "Game::redrawDialogueSprite(%d)", num);
	decodeLzss(_dialogueFrameSpriteData + 2, _tempDecodeBuffer);
	uint8_t *spriteBitmap = _tempDecodeBuffer + 25000;
	decodeLzss(_dialogueSpriteDataTable[num][_dialogueSpriteCurrentFrameTable[num]], spriteBitmap);
	int sprY = getBitmapHeight(_tempDecodeBuffer) - getBitmapHeight(spriteBitmap);
	int sprX = getBitmapWidth(_tempDecodeBuffer) - getBitmapWidth(spriteBitmap);
	int frameX = 0;
	switch (num) {
	case 0:
		frameX = _dialogueBackgroundRect.x + 13;
		_dialogueTextRect.x = frameX + getBitmapWidth(_tempDecodeBuffer) + 10;
		break;
	case 1:
		frameX = _dialogueBackgroundRect.x + _dialogueBackgroundRect.w - getBitmapWidth(_tempDecodeBuffer) - 13;
		_dialogueTextRect.x = _dialogueBackgroundRect.x + 10;
		break;
	case 2:
		frameX = _dialogueBackgroundRect.x + (_dialogueBackgroundRect.w - getBitmapWidth(_tempDecodeBuffer)) / 2;
		_dialogueTextRect.x = _dialogueBackgroundRect.x;
		break;
	}
	_dialogueTextRect.w = _dialogueBackgroundRect.w - 10 - getBitmapWidth(_tempDecodeBuffer) - 13;

	int frameY = _dialogueBackgroundRect.y + (_dialogueBackgroundRect.h - getBitmapHeight(_tempDecodeBuffer)) / 2;
	_dialogueTextRect.y = _dialogueBackgroundRect.y + 10;
	_dialogueTextRect.h = _dialogueBackgroundRect.h - 20;

	if (_stub->hasBottomScreen()) {
		// 3DS: frame + animated portrait go to the bottom screen, next to the dialogue text
		static uint8_t *portrait = 0;
		static int portraitSize = 0;
		const int fw = getBitmapWidth(_tempDecodeBuffer), fh = getBitmapHeight(_tempDecodeBuffer);
		const int sw = getBitmapWidth(spriteBitmap), sh = getBitmapHeight(spriteBitmap);
		if (fw * fh > portraitSize) {
			free(portrait);
			portrait = (uint8_t *)malloc(fw * fh);
			portraitSize = portrait ? fw * fh : 0;
		}
		if (portrait) {
			memcpy(portrait, getBitmapData(_tempDecodeBuffer), fw * fh);
			const uint8_t *sp = getBitmapData(spriteBitmap);
			const int ox = sprX / 2, oy = sprY / 2; // sprite top-left inside the frame (top-down)
			for (int j = 0; j < sh; ++j) { // bottom-up rows
				const int dy = fh - 1 - oy - (sh - 1 - j);
				if (dy < 0 || dy >= fh) continue;
				for (int i = 0; i < sw; ++i) {
					const int dx = ox + i;
					// index 0 is the sprite background: keep the frame's dark backdrop (0 would show the bottom screen colour)
					if (dx >= 0 && dx < fw && sp[j * sw + i] != 0) portrait[dy * fw + dx] = sp[j * sw + i];
				}
			}
			_stub->setBottomPanel(2, portrait, fw, fh, fw);
		}
	} else {
	_stub->copyRect(frameX, frameY, getBitmapWidth(_tempDecodeBuffer), getBitmapHeight(_tempDecodeBuffer), getBitmapData(_tempDecodeBuffer), getBitmapWidth(_tempDecodeBuffer));
	frameX += sprX / 2;
	frameY += sprY / 2;
	_stub->copyRect(frameX, frameY, getBitmapWidth(spriteBitmap), getBitmapHeight(spriteBitmap), getBitmapData(spriteBitmap), getBitmapWidth(spriteBitmap));
	}
	++_dialogueSpriteCurrentFrameTable[num];
	if (_dialogueSpriteCurrentFrameTable[num] >= _dialogueSpriteFrameCountTable[num]) {
		_dialogueSpriteCurrentFrameTable[num] = 0;
	}
}

void Game::redrawDialogueBackground() {
	debug(DBG_DIALOGUE, "Game::redrawDialogueBackground()");
	sortObjects();
	int previousObject = -1;
	for (int i = 0; i < _sceneObjectsCount; ++i) {
		SceneObject *so = _sortedSceneObjectsTable[i];
		if (so->statePrev == 1 || so->statePrev == 2) {
			if (previousObject >= 0) {
				redrawObjectBoxes(previousObject, i);
			}
			previousObject = i;
			decodeLzss(_sceneObjectFramesTable[so->frameNumPrev].data, _tempDecodeBuffer);
			SceneObjectFrame *sof = &_sceneObjectFramesTable[so->frameNumPrev];
			if (so->flipPrev == 2) {
				int y = _bitmapBuffer1.h + 1 - so->yPrev - sof->hdr.h;
				drawObjectVerticalFlip(so->xPrev, y, _tempDecodeBuffer, &_bitmapBuffer1);
			} else {
				int y = _bitmapBuffer1.h + 1 - so->yPrev - sof->hdr.h;
				drawObject(so->xPrev, y, _tempDecodeBuffer, &_bitmapBuffer1);
			}
		}
	}
	if (previousObject >= 0) {
		redrawObjectBoxes(previousObject, previousObject);
	}

	if (!_stub->hasBottomScreen()) { // 3DS: the dialogue box is not drawn on the top screen
		const uint8_t *src = _bitmapBuffer1.bits + _dialogueBackgroundRect.y * _bitmapBuffer1.pitch + _dialogueBackgroundRect.x;
		_stub->copyRect(_dialogueBackgroundRect.x, _dialogueBackgroundRect.y, _dialogueBackgroundRect.w, _dialogueBackgroundRect.h, src, _bitmapBuffer1.pitch);
		_stub->darkenRect(_dialogueBackgroundRect.x, _dialogueBackgroundRect.y, _dialogueBackgroundRect.w, _dialogueBackgroundRect.h);
	}

	for (int i = 0; i < _sceneObjectsCount; ++i) {
		SceneObject *so = _sortedSceneObjectsTable[i];
		if (so->statePrev == 1) {
			SceneObjectFrame *sof = &_sceneObjectFramesTable[so->frameNumPrev];
			int y = _bitmapBuffer1.h + 1 - so->yPrev - sof->hdr.h;
			copyBufferToBuffer(so->xPrev, y, sof->hdr.w, sof->hdr.h, &_bitmapBuffer3, &_bitmapBuffer1);
		}
	}
}

// ---------------------------------------------------------------------------
// Second screen (Nintendo 3DS bottom screen, 320x240)
// Dialogue choices are re-laid out at native size so Korean text stays readable
// while the top screen shows the game scaled down to 320x240.
// ---------------------------------------------------------------------------

static const int kBottomLineH = 18;

static bool isValidKsCode(int hi, int lo) {
	if (lo < 0xA1 || lo > 0xFE) {
		return false;
	}
	return (hi >= 0xA1 && hi <= 0xAC) || (hi >= 0xB0 && hi <= 0xC8) || (hi >= 0xCA && hi <= 0xFD);
}

int Game::bottomCharAdvance(const char *p, int *bytes) const {
	const int c = (uint8_t)p[0];
	if (_textCp949 && c >= 0x80 && p[1]) {
		*bytes = 2;
		return 16;
	}
	*bytes = 1;
	return _fontCharWidth[c] + 1;
}

int Game::bottomDrawWrapped(uint8_t *buf, int x0, int y, int maxW, const char *s, uint8_t color, bool draw) {
	const char *p = s;
	while (*p) {
		const char *start = p;
		const char *lastSpace = 0;
		int w = 0;
		while (*p && *p != '\n' && *p != '\r') {
			int nb;
			const int adv = bottomCharAdvance(p, &nb);
			if (w + adv > maxW && p != start) {
				break;
			}
			if (*p == ' ') {
				lastSpace = p;
			}
			w += adv;
			p += nb;
		}
		const char *end = p;
		if (*p && *p != '\n' && *p != '\r' && lastSpace && lastSpace > start) {
			end = lastSpace;
			p = lastSpace + 1;
		}
		if (draw && y >= 0 && y + 16 <= kBottomScreenH) {
			int x = x0;
			for (const char *q = start; q < end; ) {
				int nb;
				const int adv = bottomCharAdvance(q, &nb);
				if (x + 16 <= kBottomScreenW) {
					uint8_t *dst = buf + (kBottomScreenH - 1 - y) * kBottomScreenW + x;
					if (nb == 2) {
						const int hi = (uint8_t)q[0], lo = (uint8_t)q[1];
						if (_hangulFontData && isValidKsCode(hi, lo)) {
							drawHangulGlyph(dst, kBottomScreenW, _hangulFontData, _hangulFontLutOffset, (hi << 8) | lo, color);
						} else {
							drawChar(dst, kBottomScreenW, _fontData, '?', color);
						}
					} else {
						drawChar(dst, kBottomScreenW, _fontData, (uint8_t)q[0], color);
					}
				}
				x += adv;
				q += nb;
			}
		}
		y += kBottomLineH;
		while (*p == ' ' || *p == '\n' || *p == '\r') {
			++p;
		}
	}
	return y;
}

static void bottomHLine(uint8_t *buf, int y, uint8_t color) {
	if (y >= 0 && y < kBottomScreenH) {
		memset(buf + (kBottomScreenH - 1 - y) * kBottomScreenW + 8, color, kBottomScreenW - 16);
	}
}

void Game::drawBottomDialogue() {
	static uint8_t buf[kBottomScreenW * kBottomScreenH];
	memset(buf, kBottomColorBg, sizeof(buf));
	const bool ko = _textCp949 && _hangulFontData;
	bottomDrawWrapped(buf, 8, 4, kBottomScreenW - 16, ko ? kKoDlgHdr : kEnDlgHdr, kBottomColorTitle, true);
	bottomHLine(buf, 24, kBottomColorDim);

	static const int kTop = 30;
	const int kTextX = 20 + kBottomPortraitW; // the portrait panel takes the left column
	const int maxW = kBottomScreenW - kTextX - 8;
	const int count = MIN(_dialogueChoiceCounter, 16);
	int heights[16];
	for (int i = 0; i < count; ++i) {
		heights[i] = bottomDrawWrapped(buf, kTextX, 0, maxW, _dialogueChoiceText[i], 0, false) + 4;
		_bottomChoiceY0[i] = _bottomChoiceY1[i] = -1;
	}
	// scroll so that the highlighted choice is fully visible
	int first = 0;
	const int sel = (_dialogueSpeechIndex >= 0 && _dialogueSpeechIndex < count) ? _dialogueSpeechIndex : 0;
	for (;;) {
		int bottom = kTop;
		for (int i = first; i <= sel; ++i) {
			bottom += heights[i];
		}
		if (bottom <= kBottomScreenH || first >= sel) {
			break;
		}
		++first;
	}
	int y = kTop;
	for (int i = first; i < count && y < kBottomScreenH; ++i) {
		const uint8_t color = (i == sel) ? kBottomColorSelected : kBottomColorText;
		_bottomChoiceY0[i] = y;
		if (i == sel && count > 1) {
			drawChar(buf + (kBottomScreenH - 1 - y) * kBottomScreenW + kTextX - 14, kBottomScreenW, _fontData, '>', color);
		}
		y = bottomDrawWrapped(buf, kTextX, y, maxW, _dialogueChoiceText[i], color, true) + 4;
		_bottomChoiceY1[i] = y;
	}
	_stub->copyBottomScreen(buf, kBottomScreenW);
}

void Game::handleBottomTouch() {
	if (!_stub->_touchPressed) {
		return;
	}
	_stub->_touchPressed = false;
	const int ty = _stub->_touchY;
	if (_dialogueChoiceSelected == 0 && _dialogueChoiceCounter > 1) {
		for (int i = 0; i < MIN(_dialogueChoiceCounter, 16); ++i) {
			if (_bottomChoiceY0[i] >= 0 && ty >= _bottomChoiceY0[i] && ty < _bottomChoiceY1[i]) {
				if (i == _dialogueSpeechIndex) {
					_stub->_pi.enter = true; // second tap confirms
				} else {
					_dialogueSpeechIndex = i;
				}
				return;
			}
		}
	} else {
		_stub->_pi.enter = true; // tap to skip the current speech
	}
}

void Game::drawBottomScreen(const char *status) {
	static uint8_t buf[kBottomScreenW * kBottomScreenH];
	static char lastStatus[16];
	if (status) {
		strncpy(lastStatus, status, sizeof(lastStatus) - 1);
	}
	memset(buf, kBottomColorBg, sizeof(buf));
	const bool ko = _textCp949 && _hangulFontData;
	const char *title = _bottomHelp ? (ko ? kKoControls : kEnControls) : (ko ? kKoTitle : kEnTitle);
	int titleW = 0;
	for (const char *p = title; *p; ) {
		int nb;
		titleW += bottomCharAdvance(p, &nb);
		p += nb;
	}
	bottomDrawWrapped(buf, (kBottomScreenW - titleW) / 2, 6, kBottomScreenW, title, kBottomColorTitle, true);
	bottomHLine(buf, 26, kBottomColorDim);
	const char *const *help = ko ? kKoHelp : kEnHelp;
	int y = 30;
	for (int i = 0; _bottomHelp && help[i]; ++i) {
		// the last line (the credit) is highlighted
		const uint8_t hc = help[i + 1] ? kBottomColorText : kBottomColorSelected;
		y = bottomDrawWrapped(buf, 14, y, kBottomScreenW - 22, help[i], hc, true);
	}
	bottomHLine(buf, 214, kBottomColorDim);
	char line[96];
	const char *msg = "";
	if (strcmp(lastStatus, "saved") == 0) {
		msg = ko ? kKoSaved : kEnSaved;
	} else if (strcmp(lastStatus, "loaded") == 0) {
		msg = ko ? kKoLoaded : kEnLoaded;
	} else if (strcmp(lastStatus, "loadfail") == 0) {
		msg = ko ? kKoLoadfail : kEnLoadfail;
	} else if (strcmp(lastStatus, "savefail") == 0) {
		msg = ko ? kKoSavefail : kEnSavefail;
	}
	snprintf(line, sizeof(line), "%s %d%s%s", ko ? kKoSlot : kEnSlot, _stateSlot, msg[0] ? "  - " : "", msg);
	bottomDrawWrapped(buf, 14, 220, kBottomScreenW - 22, line, msg[0] ? kBottomColorSelected : kBottomColorDim, true);
	_stub->copyBottomScreen(buf, kBottomScreenW);
}
