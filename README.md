
Tundra, a build system
=============================================================================

Tundra is a high-performance code build system designed to give the best
possible incremental build times even for very large software projects.

Tundra is portable and works on

  - Mac OSX (10.6-10.8 tested, but most any version should work)
  - Linux
  - FreeBSD
  - Windows (Vista and later, 64 bit native)

Porting to UNIX-like platforms will be very easy, porting to other platforms
will take a little bit of work in a few well-defined places.

See doc/manual.asciidoc for more detailed usage information.

There is a companion Visual Studio 2012 addin that might be useful. See
https://github.com/deplinenoise/tundra-vsplugin for details.

Binaries
-----------------------------------------------------------------------------

Windows installers are available for download:

- [2.0 beta 2](http://tundra2-builds.s3.amazonaws.com/Tundra-Setup-Beta2.exe)
  MD5: `3e7f7018327ea523622cae6634c25367`
- [2.0 beta 1](http://tundra2-builds.s3.amazonaws.com/Tundra-Setup-Beta1.exe)
  MD5: `7db611ad5e69c518e524195493ab8880`

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
