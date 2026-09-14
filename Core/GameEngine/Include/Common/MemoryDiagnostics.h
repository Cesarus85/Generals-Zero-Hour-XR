/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// GeneralsX @feature Android port 13/07/2026 GitHub issue #5: a real-device
// report from a low-end (Unisoc) phone showed startup dying mid-line inside
// [INI] loadDirectory('Data\INI\Roads') with no C++ exception and no
// crash.log entry -- i.e. not something our exception handlers or the
// SIGSEGV-based AndroidCrashHandler can ever see. That signature (abrupt,
// unlogged, mid-syscall) is consistent with the Android low-memory killer
// sending SIGKILL, which no userspace handler can intercept by design.
// This doesn't fix that -- it can't be fixed by catching anything -- but it
// gives the NEXT report from a low-RAM device a concrete memory trend
// instead of silence, so an OOM kill becomes visible instead of just
// looking like the log stopped for no reason.
#if defined(__ANDROID__) || defined(__linux__)
#include <cstdio>
#include <cstring>

inline void LogMemoryUsageRSS(const char *tag)
{
	FILE *f = fopen("/proc/self/status", "r");
	if (f == nullptr) {
		return;
	}
	char line[256];
	while (fgets(line, sizeof(line), f) != nullptr) {
		if (strncmp(line, "VmRSS:", 6) == 0) {
			// Line looks like "VmRSS:\t   123456 kB\n" -- keep it as-is minus
			// the trailing newline, simplest way to not fight the field width.
			size_t len = strlen(line);
			while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
				line[--len] = '\0';
			}
			fprintf(stderr, "[MEM] %s: %s\n", tag, line);
			fflush(stderr);
			break;
		}
	}
	fclose(f);
}
#else
inline void LogMemoryUsageRSS(const char * /*tag*/) {}
#endif
