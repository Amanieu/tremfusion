/*
===========================================================================
Copyright (C) 2009 Amanieu d'Antras

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

printHandler_t PrintfHook = NULL;

// We need to leave room for enough print handlers
#define NUM_PRINT_HANDLERS 8
static printHandler_t handlers[NUM_PRINT_HANDLERS] = {NULL};

void _VPrintf(printLevel_t level, const char *fmt, va_list ap)
{
	int i;
	char printbuf[MAX_PRINTMSG];
#ifdef _WIN32
	char fmtbuf[MAX_PRINTMSG];
	char *find;

	// Fix format string for non-ISO compliant win32 C library
	strncpy(fmtbuf, fmt, sizeof(fmtbuf));
	while ((find = strstr(fmtbuf, "%zd")))
		find[1] = 'I';
	while ((find = strstr(fmtbuf, "%zi")))
		find[1] = 'I';
	while ((find = strstr(fmtbuf, "%zu")))
		find[1] = 'I';
	fmt = fmtbuf;
#endif

	Q_vsnprintf(printbuf, sizeof(printbuf), fmt, ap);

	if (PrintfHook) {
		PrintfHook(level, printbuf);
		return;
	}

	for (i = 0; i < NUM_PRINT_HANDLERS; i++) {
		if (handlers[i])
			handlers[i](level, printbuf);
	}
}

void _Printf(printLevel_t level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Com_VPrintf(level, fmt, ap);
	va_end(ap);
}

void RegisterPrintHandler(printHandler_t handler)
{
	int i;

	for (i = 0; i < NUM_PRINT_HANDLERS; i++) {
		if (!handlers[i]) {
			handlers[i] = handler;
			return;
		}
	}

	Error("Too many print handlers");
}

void RemovePrintHandler(printHandler_t handler)
{
	int i;

	for (i = 0; i < NUM_PRINT_HANDLERS; i++) {
		if !handlers[i] == handler) {
			handlers[i] = NULL;
			return;
		}
	}
}

void Error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Com_VPrintf(PRINT_ERROR, fmt, ap);
	va_end(ap);

	// Break to the debugger if any, otherwise let the signal handler clean up
	abort();
}

void Warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Com_VPrintf(PRINT_WARNING, fmt, ap);
	va_end(ap);
}

void Msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Com_VPrintf(PRINT_MESSAGE, fmt, ap);
	va_end(ap);
}

void Debug(const char *group, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Com_VPrintf(PRINT_DEBUG, fmt, ap);
	va_end(ap);
}
