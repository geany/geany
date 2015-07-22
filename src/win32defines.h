/*
 *      win32defines.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef WIN32DEFINES_H
#define WIN32DEFINES_H 1

/* These defines are necessary defines *before* windows.h is included */

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
/* Need Windows XP for SHGetFolderPathAndSubDirW */
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
/* Needed for SHGFP_TYPE */
#define _WIN32_IE 0x0500

#endif /* WIN32DEFINES_H */
