/*
   Copyright 2010 Andreas Fredriksson

   This file is part of Tundra.

   Tundra is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Tundra is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tundra.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TUNDRA_CONFIG_H
#define TUNDRA_CONFIG_H

#if defined(__APPLE__)
#define TUNDRA_UNIX 1
#define TUNDRA_APPLE 1
#elif defined(_WIN32)
#define TUNDRA_WIN32 1
#elif defined(linux)
#define TUNDRA_UNIX 1
#define TUNDRA_LINUX 1
#elif defined(__FreeBSD__)
#define TUNDRA_UNIX 1
#define TUNDRA_FREEBSD 1
#elif defined(__OpenBSD__)
#define TUNDRA_UNIX 1
#define TUNDRA_OPENBSD 1
#else
#error Unsupported OS
#endif


#endif
