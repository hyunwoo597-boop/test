/*
 * Bermuda Syndrome engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir
 */

#include <strings.h>
#include <sys/param.h>
#include "decoder.h"
#include "file.h"
#include "fs.h"
#include "game.h"
#include "loadlog.h"
#include "str.h"

static const bool kDumpObjectScript = false;

void Game::allocateTables() {
	_tempDecodeBuffer = (uint8_t *)malloc(65535);
	if (!_tempDecodeBuffer) {
		error("Unable to allocate temporary decode buffer (65535 bytes)");
	}
	_bitmapBuffer0 = (uint8_t *)malloc(kBitmapBufferDefaultSize);
	if (!_bitmapBuffer0) {
		error("Unable to allocate bitmap buffer 0 (%d bytes)", kBitmapBufferDefaultSize);
	}
	_bitmapBuffer2 = (uint8_t *)malloc(kBitmapBufferDefaultSize);
	if (!_bitmapBuffer2) {
		error("Unable to allocate bitmap buffer 2 (%d bytes)", kBitmapBufferDefaultSize);
	}
}

void Game::deallocateTables() {
	if (_bitmapBuffer0) {
		free(_bitmapBuffer0);
		_bitmapBuffer0 = 0;
	}
	if (_bitmapBuffer2) {
		free(_bitmapBuffer2);
		_bitmapBuffer2 = 0;
	}
	if (_tempDecodeBuffer) {
		free(_tempDecodeBuffer);
		_tempDecodeBuffer = 0;
	}
}

void Game::loadCommonSprites() {
	if (_isDemo) {
		_bermudaOvrData = 0;
	} else {
		_bermudaOvrData = loadFile("..\\bermuda.ovr");
	}

	loadWGP("..\\bermuda.wgp");
	_bagBackgroundImage = _bitmapBuffer1;
	int bagBitmapSize = _bagBackgroundImage.pitch * (_bagBackgroundImage.h + 1);
	_bagBackgroundImage.bits = (uint8_t *)malloc(bagBitmapSize);
	if (!_bagBackgroundImage.bits) {
		error("Unable to allocate bag bitmap buffer (%d bytes)", bagBitmapSize);
	}
	memcpy(_bagBackgroundImage.bits, _bitmapBuffer1.bits, bagBitmapSize);

	loadFile("..\\bermuda.spr", _tempDecodeBuffer);
	int decodedSize = READ_LE_UINT32(_tempDecodeBuffer + 2);
	_bermudaSprData = (uint8_t *)malloc(decodedSize);
	if (!_bermudaSprData) {
		error("Unable to allocate bermuda.spr buffer (%d bytes)", decodedSize);
	}
	decodeLzss(_tempDecodeBuffer + 2, _bermudaSprData);
	_bermudaSprDataTable[0] = _bermudaSprData;
	for (int i = 1; i < 3; ++i) {
		_bermudaSprDataTable[i] = _bermudaSprDataTable[i - 1] + getBitmapSize(_bermudaSprDataTable[i - 1]);
	}
	for (int i = 0; i < 11; ++i) {
		if (i == 0) {
			_lifeBarImageTable[i][0] = _bermudaSprDataTable[2] + getBitmapSize(_bermudaSprDataTable[2]);
		} else {
			_lifeBarImageTable[i][0] = _lifeBarImageTable[i - 1][11] + getBitmapSize(_lifeBarImageTable[i - 1][11]);
		}
		for (int j = 1; j < 12; ++j) {
			_lifeBarImageTable[i][j] = _lifeBarImageTable[i][j - 1] + getBitmapSize(_lifeBarImageTable[i][j - 1]);
		}
	}
	_bagObjectAreaBlinkImageTable[0] = _lifeBarImageTable[10][11] + getBitmapSize(_lifeBarImageTable[10][11]);
	for (int i = 1; i < 10; ++i) {
		_bagObjectAreaBlinkImageTable[i] = _bagObjectAreaBlinkImageTable[i - 1] + getBitmapSize(_bagObjectAreaBlinkImageTable[i - 1]);
	}
	_weaponIconImageTable[0] = _bagObjectAreaBlinkImageTable[9] + getBitmapSize(_bagObjectAreaBlinkImageTable[9]);
	for (int i = 1; i < 14; ++i) {
		_weaponIconImageTable[i] = _weaponIconImageTable[i - 1] + getBitmapSize(_weaponIconImageTable[i - 1]);
	}
	for (int i = 0; i < 2; ++i) {
		if (i == 0) {
			_ammoIconImageTable[i][0] = _weaponIconImageTable[13] + getBitmapSize(_weaponIconImageTable[13]);
		} else {
			_ammoIconImageTable[i][0] = _ammoIconImageTable[i - 1][4] + getBitmapSize(_ammoIconImageTable[i - 1][4]);
		}
		for (int j = 1; j < 5; ++j) {
			_ammoIconImageTable[i][j] = _ammoIconImageTable[i][j - 1] + getBitmapSize(_ammoIconImageTable[i][j - 1]);
		}
	}
	if (_isDemo) {
		_swordIconImage = 0;
		_iconBackgroundImage = _ammoIconImageTable[1][4] + getBitmapSize(_ammoIconImageTable[1][4]);
		_lifeBarImage = _iconBackgroundImage + getBitmapSize(_iconBackgroundImage);
		memset(_bagWeaponAreaBlinkImageTable, 0, sizeof(_bagWeaponAreaBlinkImageTable));
	} else {
		_swordIconImage = _ammoIconImageTable[1][4] + getBitmapSize(_ammoIconImageTable[1][4]);
		_iconBackgroundImage = _swordIconImage + getBitmapSize(_swordIconImage);
		_lifeBarImage = _iconBackgroundImage + getBitmapSize(_iconBackgroundImage);
		_bagWeaponAreaBlinkImageTable[0] = _lifeBarImage + getBitmapSize(_lifeBarImage);
		for (int i = 1; i < 10; ++i) {
			_bagWeaponAreaBlinkImageTable[i] = _bagWeaponAreaBlinkImageTable[i - 1] + getBitmapSize(_bagWeaponAreaBlinkImageTable[i - 1]);
		}
	}
}

void Game::unloadCommonSprites() {
	if (_bermudaOvrData) {
		free(_bermudaOvrData);
		_bermudaOvrData = 0;
	}
	if (_bagBackgroundImage.bits) {
		free(_bagBackgroundImage.bits);
		_bagBackgroundImage.bits = 0;
	}
	if (_bermudaSprData) {
		free(_bermudaSprData);
		_bermudaSprData = 0;
	}
}

uint8_t *Game::loadFile(const char *fileName, uint8_t *dst, uint32_t *dstSize) {
	debug(DBG_RES, "Game::loadFile('%s')", fileName);
	FileHolder fp(_fs, fileName);
	uint32_t fileSize = fp->size();
	if (!dst) {
		dst = (uint8_t *)malloc(fileSize);
		if (!dst) {
			error("Unable to allocate buffer for file loading (%d bytes)", fileSize);
		}
	}
	if (dstSize) {
		*dstSize = fileSize;
	}
	fp->read(dst, fileSize);
	return dst;
}

void Game::loadWGP(const char *fileName) {
	LoadTimer loadTime(_stub, "wgp", fileName);
	debug(DBG_RES, "Game::loadWGP('%s')", fileName);
	FileHolder fp(_fs, fileName);
	int offs = kOffsetBitmapBits;
	int len = 0;
	int tag = fp->readUint16LE();
	if (tag == 0x4D42) { // _10.SCN (uncompressed .bmp)
		len = fp->readUint32LE() - 14;
		fp->seek(8, SEEK_CUR);
		fp->read(_bitmapBuffer0, len);
	} else if (tag == 0x5057) {
		len = 0;
		int dataSize = fp->size();
		do {
			const int sz = fp->readUint16LE();
			if (fp->ioErr()) {
				break;
			} else if (sz != 0) {
				fp->read(_bitmapBuffer2, sz);
				const int decodedSize = decodeLzss(_bitmapBuffer2, _bitmapBuffer0 + len);
				len += decodedSize;
				dataSize -= sz;
			}
			dataSize -= 2;
		} while (dataSize > 0);
		offs += 4;
		len += 4;
	} else if (tag == 0x505A) {
		const int sz = fp->size() - 2;
		fp->read(_bitmapBuffer2, sz);
		len = decodeZlib(_bitmapBuffer2, _bitmapBuffer0);
	} else {
		error("Invalid wgp format %X", tag);
	}
	_loadDataState = 1;

	_bitmapBuffer1.w = READ_LE_UINT32(_bitmapBuffer0 + 4) - 1;
	_bitmapBuffer1.h = READ_LE_UINT32(_bitmapBuffer0 + 8) - 1;
	_bitmapBuffer1.pitch = (READ_LE_UINT32(_bitmapBuffer0 + 4) + 3) & ~3;
	_bitmapBuffer1.bits = _bitmapBuffer2;
	_bitmapBuffer3 = _bitmapBuffer1;
	_bitmapBuffer3.bits = _bitmapBuffer0 + offs;
	memcpy(_bitmapBuffer1.bits, _bitmapBuffer3.bits, len - offs);
}

static inline int readSprU16(const uint8_t *&p, const uint8_t *end) {
	if (end - p < 2) {
		p = end;
		return 0;
	}
	const int v = p[0] | (p[1] << 8);
	p += 2;
	return v;
}

// the raw sprite files of the recent scenes stay in memory: a scene that uses the same characters again does
// not read them from the SD card
static const uint32_t kSprCacheLimit = 10 * 1024 * 1024;
static const int kSprCacheSlots = 16;
struct SprCacheEntry {
	char name[160];
	uint8_t *data;
	uint32_t size;
	uint32_t used;
};
static SprCacheEntry _sprCache[kSprCacheSlots];
static uint32_t _sprCacheTotal;
static uint32_t _sprCacheTick;

static const uint8_t *sprCacheGet(const char *name, uint32_t *size) {
	for (int i = 0; i < kSprCacheSlots; ++i) {
		SprCacheEntry *e = &_sprCache[i];
		if (e->data && strcasecmp(e->name, name) == 0) {
			e->used = ++_sprCacheTick;
			*size = e->size;
			return e->data;
		}
	}
	return 0;
}

// returns true when the cache took the buffer
static bool sprCachePut(const char *name, uint8_t *data, uint32_t size) {
	if (size > kSprCacheLimit || strlen(name) >= sizeof(_sprCache[0].name)) {
		return false;
	}
	while (1) {
		int freeSlot = -1, oldest = -1;
		for (int i = 0; i < kSprCacheSlots; ++i) {
			SprCacheEntry *e = &_sprCache[i];
			if (!e->data) {
				if (freeSlot < 0) freeSlot = i;
			} else if (oldest < 0 || e->used < _sprCache[oldest].used) {
				oldest = i;
			}
		}
		if (freeSlot >= 0 && _sprCacheTotal + size <= kSprCacheLimit) {
			SprCacheEntry *e = &_sprCache[freeSlot];
			strcpy(e->name, name);
			e->data = data;
			e->size = size;
			e->used = ++_sprCacheTick;
			_sprCacheTotal += size;
			return true;
		}
		if (oldest < 0) {
			return false;
		}
		SprCacheEntry *e = &_sprCache[oldest];
		_sprCacheTotal -= e->size;
		free(e->data);
		e->data = 0;
		e->size = 0;
	}
}

void Game::loadSPR(const char *fileName, SceneAnimation *sa) {
	debug(DBG_RES, "Game::loadSPR('%s')", fileName);
	// the file is read once (or taken from the cache) and parsed from memory: reading every field with its own
	// call to the file layer was the slow part of a scene change on the 3DS SD card
	uint32_t size = 0;
	const uint8_t *buf = sprCacheGet(fileName, &size);
	bool owned = false;
	if (!buf) {
		FileHolder fp(_fs, fileName);
		size = fp->size();
		uint8_t *data = (uint8_t *)malloc(size > 0 ? size : 1);
		if (!data) {
			error("Unable to allocate %d bytes", size);
		}
		fp->read(data, size);
		if (sprCachePut(fileName, data, size)) {
			buf = data;
		} else {
			buf = data;
			owned = true;
		}
	}
	const uint8_t *p = buf;
	const uint8_t *end = buf + size;
	int (*decode)(const uint8_t *, uint8_t *) = 0;
	const int tag = readSprU16(p, end);
	if (tag == 0x3553) {
		decode = decodeLzss;
	} else if (tag == 0x355A) {
		decode = decodeZlib;
	} else {
		error("Invalid spr format %X", tag);
	}
	sa->motionsCount = 0;
	sa->firstMotionIndex = _sceneObjectMotionsCount;
	while (1) {
		int num = readSprU16(p, end);
		if (num == 0) {
			break;
		}
		assert(_sceneObjectMotionsCount < NUM_SCENE_MOTIONS);
		SceneObjectMotion *motion = &_sceneObjectMotionsTable[_sceneObjectMotionsCount];
		motion->firstFrameIndex = _sceneObjectFramesCount;
		motion->count = num;
		motion->animNum = _animationsCount;
		assert(_sceneObjectFramesCount + num <= NUM_SCENE_OBJECT_FRAMES);
		for (int i = 0; i < num; ++i) {
			int len = readSprU16(p, end);
			if (end - p < len) {
				len = (int)(end - p);
			}
			SceneObjectFrame *frame = &_sceneObjectFramesTable[_sceneObjectFramesCount];
			frame->data = (uint8_t *)malloc(len > 0 ? len : 1);
			if (!frame->data) {
				error("Unable to allocate %d bytes", len);
			}
			memcpy(frame->data, p, len);
			p += len;
			frame->hdr.num = readSprU16(p, end);
			frame->hdr.w = readSprU16(p, end);
			frame->hdr.h = readSprU16(p, end);
			frame->hdr.xPos = readSprU16(p, end);
			frame->hdr.yPos = readSprU16(p, end);
			frame->decode = decode;
			++_sceneObjectFramesCount;
		}
		++sa->motionsCount;
		++_sceneObjectMotionsCount;
	}
	if (owned) {
		free((void *)buf);
	}
}

static void dumpObjectScript(SceneAnimation *sa, const char *dirPath, const char *fileName) {
	const char *name = strrchr(fileName, '\\');
	if (name) {
		char filePath[512];
		snprintf(filePath, sizeof(filePath), "%s/dumps/%s.script", dirPath, name + 1);
		File f;
		if (f.open(filePath, "wb")) {
			f.write(sa->scriptData, sa->scriptSize);
			f.close();
		}
	}
}

void Game::loadMOV(const char *fileName) {
	LoadTimer loadTime(_stub, "mov", fileName);
	debug(DBG_RES, "Game::loadMOV('%s')", fileName);
	FileHolder fp(_fs, fileName);
	int tag = fp->readUint16LE();
	if (tag != 0x354D) {
		error("Invalid mov format %X", tag);
	}
	assert(_animationsCount < NUM_SCENE_ANIMATIONS);
	SceneAnimation *sa = &_animationsTable[_animationsCount];

	char sprName[128];
	int len = fp->readUint16LE();
	fp->read(sprName, len);

	char wgpName[128];
	len = fp->readUint16LE();
	fp->read(wgpName, len);

	int speed = fp->readUint16LE(); // anim speed in ms
	debug(DBG_RES, "speed %d ms", speed);

	if (_loadDataState == 0) {
		loadWGP(wgpName);
		strcpy(_currentSceneWgp, wgpName);
		_sceneObjectFramesCount = 0;
		_sceneObjectMotionsCount = 0;
		_sceneObjectsCount = 0;
		for (int i = 0; i < NUM_SCENE_OBJECTS; ++i) {
			memset(_sceneObjectsTable[i].varsTable, 0, sizeof(_sceneObjectsTable[i].varsTable));
		}
	}
	sa->unk26 = 0;
	sa->objectsCount = 0;
	sa->firstObjectIndex = _sceneObjectsCount;
	sa->soundBuffersCount = 0;
	sa->firstSoundBufferIndex = _soundBuffersCount;
	while (1) {
		int type = fp->readUint16LE();
		switch (type) {
		case 1:
			while (1) {
				len = fp->readUint16LE();
				if (len == 0) {
					break;
				}
				assert(_soundBuffersCount < NUM_SOUND_BUFFERS);
				fp->read(_soundBuffersTable[_soundBuffersCount].filename, len);
				++_soundBuffersCount;
				++sa->soundBuffersCount;
			}
			break;
		case 2:
			while (1) {
				int index = fp->readUint16LE();
				if (index == 0) {
					break;
				}
				--index;
				Box *box = derefBox(index, _boxesCountTable[index]);
				box->state = fp->readByte();
				box->x1 = fp->readUint16LE();
				box->y1 = fp->readUint16LE();
				box->x2 = fp->readUint16LE();
				box->y2 = fp->readUint16LE();
				++_boxesCountTable[index];
			}
			break;
		case 3: {
				assert(_sceneObjectsCount < NUM_SCENE_OBJECTS);
				SceneObject *so = &_sceneObjectsTable[_sceneObjectsCount];
				len = fp->readUint16LE();
				memset(so->name, 0, sizeof(so->name)); // some objects have an empty name: do not keep the previous slot's name
				fp->read(so->name, len);
				for (int i = 0; i < _sceneObjectsCount && so->name[0]; ++i) {
					if (strcmp(_sceneObjectsTable[i].name, so->name) == 0) {
						// lookups use the first match, so a second object with the same name is harmless;
						// a fatal error here ended the game on the 3DS ("Duplicate object name MENU")
						warning("Duplicate object name %s", so->name);
						break;
					}
				}
				so->motionFrameNum = 0;
				so->motionNum = 0,
				so->flipInit = 0;
				so->zInit = 0;
				so->yInit = 0;
				so->xInit = 0;
				so->mode = 0;
				so->statePrev = 0;
				so->state = 0;
				so->motionInit = _sceneObjectMotionsCount;
				while (1) {
					int initType = fp->readUint16LE();
					if (initType == 0) {
						break;
					}
					switch (initType) {
					case 2000:
						len = fp->readUint16LE();
						fp->read(so->className, len);
						break;
					case 3000:
						so->mode = fp->readUint16LE();
						if (so->mode == 2) {
							so->modeRndMul = fp->readUint16LE();
						}
						break;
					case 3500:
						so->xInit = fp->readUint16LE();
						so->yInit = fp->readUint16LE();
						break;
					case 4000:
						so->zInit = fp->readUint16LE();
						break;
					case 4500:
						so->flipInit = fp->readUint16LE();
						break;
					case 5000:
						so->motionFrameNum = fp->readUint16LE();
						--so->motionFrameNum;
						break;
					case 5500:
						so->motionNum = fp->readUint16LE();
						--so->motionNum;
						break;
					case 6000: {
							int var = fp->readUint16LE();
							assert(var >= 0 && var < 10);
							so->varsTable[var] = fp->readUint16LE();
						}
						break;
					default:
						assert(0);
						break;
					}
				}
				++_sceneObjectsCount;
				++sa->objectsCount;
			}
			break;
		case 4:
			sa->scriptSize = fp->readUint16LE();
			if (sa->scriptSize != 0) {
				sa->scriptData = (uint8_t *)malloc(sa->scriptSize);
				fp->read(sa->scriptData, sa->scriptSize);
				if (kDumpObjectScript) {
					dumpObjectScript(sa, _savePath, fileName);
				}
			}
			break;
		case 5:
			sa->unk26 = fp->readUint16LE();
			break;
		default:
			assert(0);
		}
		if (type == 4) {
			break;
		}
	}

	char filePath[128];
	strcpy(filePath, fileName);
	char *p = strrchr(filePath, '\\');
	if (p) {
		strcpy(p + 1, sprName);
	} else {
		strcpy(filePath, sprName);
	}

	loadSPR(filePath, sa);

	strcpy(sa->name, fileName);
	stringToLowerCase(sa->name);

	++_animationsCount;

//	for (int i = sa->firstSoundBufferIndex; i < _soundBuffersCount; ++i) {
//		SoundBuffer *sb = &_soundBuffersTable[i];
//		sb->buffer = loadFile(sb->filename);
//	}

	_loadDataState = 2;
//	_skipUpdateScreen = false;
}

void Game::loadKBR(const char *fileName) {
	_keyboardReplaySize = 0;
	_keyboardReplayOffset = 0;
	free(_keyboardReplayData);
	_keyboardReplayData = 0;

	char name[128];
	strcpy(name, fileName);
	char *p = strstr(name, ".SAV");
	if (p) {
		strcpy(p, ".KBR");
		FileHolder fh(_fs, name, false);
		if (fh._fp) {
			_keyboardReplaySize = fh->size();
			if (_keyboardReplaySize & 127) {
				warning("Unexpected keyboard buffer size %d", _keyboardReplaySize);
			}
			_keyboardReplayData = (uint8_t *)malloc(_keyboardReplaySize);
			if (_keyboardReplayData) {
				fh->read(_keyboardReplayData, _keyboardReplaySize);
				debug(DBG_RES, "Loading keyboard replay buffer size %d", _keyboardReplaySize);
			}
		} else {
			warning("Unable to open '%s' for reading", name);
		}
	}
}

void Game::loadTBM() {
	static const char *name = "BT16.TBM";
	File f;
	bool opened = f.open(name);
	if (!opened) {
		char path[MAXPATHLEN];
		snprintf(path, sizeof(path), "%s/%s", _savePath, name);
		opened = f.open(path);
		if (!opened) {
			snprintf(path, sizeof(path), "%s/%s", _dataPath, name);
			opened = f.open(path);
		}
	}
	if (opened) {
		const uint32_t dataSize = f.size();
		uint8_t *p = (uint8_t *)malloc(dataSize);
		if (!p) {
			warning("Unable to allocate %d bytes for '%s'", dataSize, name);
			return;
		}
		f.read(p, dataSize);
		if (memcmp(p, "TTBITMAP VARIABLE-PITCH HANGEUL", 31) != 0) {
			warning("Unexpected signature for '%s'", name);
			free(p);
			return;
		}
		const int glyphW = READ_LE_UINT16(p + 32);
		const int glyphH = READ_LE_UINT16(p + 34);
		if (glyphW != 16 || glyphH != 16) {
			warning("Unexpected glyph size %d,%d", glyphW, glyphH);
			free(p);
			return;
		}
		const int count = READ_LE_UINT16(p + 36);
		_hangulFontLutOffset = 38 + count * 16;
		_hangulFontData = p;
	} else {
		warning("Unable to open Hangul font '%s'", name);
		_hangulFontData = 0;
	}
}
