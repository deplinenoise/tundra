
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

- 2.0 Beta4 (2013-06-16)
  - [installer](http://tundra2-builds.s3.amazonaws.com/Tundra-Setup-Beta4.exe) MD5: `ffbe55fd9dcf7a009f6264e6cf8824b5`
  - [zip](http://tundra2-builds.s3.amazonaws.com/Tundra-Binaries-Beta4.zip) MD5: `6ff1c6e87f30325fac295ebed5b9d07b`
- 2.0 Beta3 (2013-05-09)
  - [installer](http://tundra2-builds.s3.amazonaws.com/Tundra-Setup-Beta3.exe) MD5: `5f3b740259646d24eaf321b1b430836e`
  - [zip](http://tundra2-builds.s3.amazonaws.com/Tundra-Binaries-Beta3.zip) MD5: `dd8ebd0f9d461b3eea2192d02016cdb8`


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
