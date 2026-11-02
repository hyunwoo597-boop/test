/*
 * 3DS: timing of the scene changes. Each measured step appends one line to load_log.txt in the
 * working directory (sdmc:/3ds/bermuda). Other builds compile this to nothing.
 */
#ifndef LOADLOG_H__
#define LOADLOG_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "systemstub.h"

static inline void loadLogWrite(const char *what, const char *arg, uint32_t ms) {
#ifdef __3DS__
	FILE *f = fopen("load_log.txt", "a");
	if (f) {
		fprintf(f, "%s %s %u ms\n", what, arg, (unsigned)ms);
		fclose(f);
	}
#else
	(void)what; (void)arg; (void)ms;
#endif
}

struct LoadTimer {
	SystemStub *_stub;
	const char *_what;
	char _arg[96];
	uint32_t _t0;
	LoadTimer(SystemStub *stub, const char *what, const char *arg) : _stub(stub), _what(what), _t0(stub->getTimeStamp()) {
		strncpy(_arg, arg ? arg : "", sizeof(_arg) - 1);
		_arg[sizeof(_arg) - 1] = 0;
	}
	~LoadTimer() {
		loadLogWrite(_what, _arg, _stub->getTimeStamp() - _t0);
	}
};

#endif
