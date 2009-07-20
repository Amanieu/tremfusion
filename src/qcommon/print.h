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

// Printing and error handling functions

#ifndef __PRINT_H
#define __PRINT_H

// Maximum length of a string passed to Com_Printf (above gets truncated)
#define	MAX_PRINTMSG 4096

// Print level which corresponds to the importance of the message
typedef enum {
	PRINT_ERROR,
	PRINT_WARNING,
	PRINT_MESSAGE,
	PRINT_DEBUG
} printLevel_t;

// Function that will handle a print
typedef void (*printHandler_t)(printLevel_t level, const char *msg);

// If this is non-NULL, all prints are routed through this function instead
// of the usual print handlers
extern printHandler_t PrintfHook;

// Generic print functions
void _Printf(printLevel_t level, const char *fmt, ...) __printf(2, 3);
void _VPrintf(printLevel_t level, const char *fmt, va_list ap);

// Register a function to handle anything that gets printed
// You must NOT call any print function from inside a print handler
void RegisterPrintHandler(printHandler_t handler);
void RemovePrintHandler(printHandler_t handler);

// Shortcuts for different print levels, Error will abort engine execution
void Error(const char *fmt, ...) __noreturn __cold __printf(1, 2);
void Warning(const char *fmt, ...) __cold __printf(1, 2);
void Msg(const char *fmt, ...) __printf(1, 2);

// Debugging helpers
void SetDebugGroup(const char *group, qboolean enable);
void Debug(const char *group, const char *fmt, ...) __printf(2, 3);
#ifdef DEBUG
#define Assert(cond) do { \
	if (!(cond)) \
		Error("Assertion failed in %s at %s:%d", __func__, __FILE__, __LINE__); \
	} while (0)
#else
#define Assert do {} while (0)
#endif

#endif
