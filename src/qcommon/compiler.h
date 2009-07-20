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

#ifndef __COMPILER_H
#define __COMPILER_H

/* Compiler-specific optimisations */

#ifdef __GNUC__

/* Emit a nice warning when a function is used */
#define __deprecated __attribute__((deprecated))

/* Pack elements in a struct to 1 byte */
#define __packed __attribute__((packed))

/* Align a variable to x bytes */
#define __aligned(x) __attribute__((aligned(x)))

/* Indicates that a function does not return */
#define __noreturn __attribute__((noreturn))

/* Pure functions depend only on their arguments, do not modify any global
 * state, and have no effect other than their return value */
#define __pure __attribute__((pure))

/* Const functions do not reference any global memory or pointers, only their
 * arguments, and have no effect other than their return value */
#define __const_func __attribute__((__const__))

/* Expect printf-style arguments for a function: a is the index of the format
 * string, and b is the index of the first variable argument */
#define __printf(a, b) __attribute__((format(printf, a, b)))

/* Allow this function to be used from other modules */
#ifdef _WIN32
#define __dllexport __attribute__((dllexport))
#else
#define __dllexport __attribute__((visibility("default")))
#endif

/* Mark that this function is imported from another module */
#ifdef _WIN32
#define __dllimport __attribute__((dllimport))
#else
#define __dllimport
#endif

/* Marks that this function will return a pointer that is not aliased to any
 * other pointer */
#define __restrict __attribute__((malloc))

#if __GNUC__ >= 4
#if __GNUC_MINOR__ >= 3

/* A cold function is rarely called, and branches that lead to one are assumed
 * to be unlikely */
#define __cold __attribute__((__cold__))

#if __GNUC_MINOR__ >= 4

/* Allows selecting a different target than the main compilation target */
#define __target(x) __attribute__((__target__(x)))

#else
#define __target(x)
#endif
#else
#define __cold
#define __target(x)
#endif
#endif

#elif _MSC_VER

#define __deprecated __declspec(deprecated)
#define __packed
#define __aligned(x) __declspec(align(x))
#define __noreturn __declspec(noreturn)
#define __pure __declspec(noalias)
#define __const_func
#define __printf(a, b)
#define __dllexport __declspec(dllexport)
#define __dllimport __declspec(dllimport)
#define __restrict __declspec(restrict)
#define __cold
#define __target(x)
// This doesn't work in dlls
#define __thread __declspec(thread)

#else

#warning "Unsupported compiler"
#define __deprecated
#define __packed
#define __aligned(x)
#define __noreturn
#define __pure
#define __const_func
#define __printf(a, b)
#define __dllexport
#define __dllimport
#define __restrict
#define __cold
#define __target(x)
#endif

#endif
