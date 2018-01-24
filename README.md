[![Build Status](https://travis-ci.org/deplinenoise/tundra.svg?branch=master)](https://travis-ci.org/deplinenoise/tundra)

Tundra, a build system
=============================================================================

Tundra is a high-performance code build system designed to give the best
possible incremental build times even for very large software projects.

Tundra is portable and works on

  - macOS
  - Linux
  - FreeBSD
  - Windows (XP or later - binary releases require Vista/64 or later - for XP support build from source using MinGW.)

Porting to UNIX-like platforms will be very easy, porting to other platforms
will take a little bit of work in a few well-defined places.

See doc/manual.asciidoc for more detailed usage information.

There is a companion Visual Studio 2012 addin that might be useful. See
https://github.com/deplinenoise/tundra-vsplugin for details.

Binaries
-----------------------------------------------------------------------------

Windows installers are available for download in the releases tab.

macOS package is available via [Homebrew](http://brew.sh): `brew install tundra`

License and Copyright
-----------------------------------------------------------------------------

Tundra is Copyright 2010-2013 Andreas Fredriksson.

Tundra is made available under the GNU GPL. See the file COPYING for the
complete license text.

Tundra uses Lua. See below for Lua's licensing terms which are compatible with
those of the GNU GPL.

Tundra includes a public domain Lua Debugger, see below.

Tundra includes the public domain dlmalloc by Doug Lea.

Lua
-----------------------------------------------------------------------------

Copyright (c) 1994-2008 Lua.org, PUC-Rio.
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Lua Debugger
-----------------------------------------------------------------------------

Tundra includes an optional Lua CLI debugger which is public domain software
written by Dave Nichols.

The debugger was obtained from [luaforge.net](http://luaforge.net/projects/clidebugger/).
