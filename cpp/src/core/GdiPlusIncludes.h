/*****************************************************************************/
/* GdiPlusIncludes.h                                                         */
/*                                                                           */
/* WARNING: DO NOT MODIFY THIS FILE                                          */
/*                                                                           */
/* GDI+ requires a specific include order. Changing it causes build errors:  */
/*   - META_*, EMR_* undeclared identifier                                   */
/*   - CALLBACK, UINT not found                                              */
/*                                                                           */
/* Correct order:                                                            */
/*   1. windows.h  - provides GDI types (META_*, EMR_*, CALLBACK)            */
/*   2. objidl.h   - provides IStream for GDI+                               */
/*   3. gdiplus.h  - uses all above types                                    */
/*                                                                           */
/* DO NOT define WIN32_LEAN_AND_MEAN before including this header.           */
/*****************************************************************************/

#ifndef GDIPLUSINCLUDES_H
#define GDIPLUSINCLUDES_H

#ifdef MSWindows

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#endif // MSWindows

#endif // GDIPLUSINCLUDES_H
