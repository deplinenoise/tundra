RM = rm -f
MKDIR = mkdir -p
INSTALL_X = install -m 0755
INSTALL_F = install -m 0644

CFLAGS ?= -Wall
CPPFLAGS = -Ilua/src -Isrc -MMD -MP -DHAVE_GIT_INFO
CXXFLAGS ?= $(CFLAGS) -fno-exceptions

CXXLIBFLAGS ?=
LDFLAGS += -L$(BUILDDIR) -ltundra

PREFIX ?= /usr/local

GIT_BRANCH := $(shell (git branch 2>/dev/null) | sed -n '/^\*/s/^\* //p')

ifeq ($(GIT_BRANCH),)
GIT_BRANCH := unknown
GIT_FILE := dummy_version_file
else
GIT_FILE := .git/refs/heads/$(GIT_BRANCH)
endif

CHECKED ?= no
ifeq ($(CHECKED), no)
CFLAGS += -O3 -DNDEBUG
BUILDDIR := build
else
CFLAGS += -g -D_DEBUG
BUILDDIR := build.checked
endif

CROSSMINGW ?= no
EXESUFFIX =

ifeq ($(CROSSMINGW), yes)
# Cross-compiling for windows on unix-like platform.
CROSS ?= x86_64-w64-mingw32-
CC := $(CROSS)gcc
CXX := $(CROSS)g++
AR := $(CROSS)ar rcus
CXXFLAGS += -std=gnu++11 
CPPFLAGS += -D_WIN32 -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -D__MSVCRT_VERSION__=0x0601 -DFORCEINLINE='__inline __attribute__((always_inline))'
BUILDDIR := build.mingw
EXESUFFIX := .exe
else
CC ?= clang
CXX ?= clang++
AR= ar rcus

# Not cross-compiling. Detect options based on uname output.
UNAME := $(shell uname)
ifeq ($(UNAME), $(filter $(UNAME), FreeBSD NetBSD OpenBSD))
CC := clang
CXX := clang++
CXXFLAGS += -std=c++11 
LDFLAGS += -lpthread
else
ifeq ($(UNAME), $(filter $(UNAME), Linux))
CC := clang
CXX := clang++
CXXFLAGS += -std=c++11 
LDFLAGS += -pthread
else
ifeq ($(UNAME), $(filter $(UNAME), Darwin))
CXXFLAGS += -std=c++11 
CXXFLAGS += -stdlib=libc++
LDFLAGS += -stdlib=libc++
else
ifeq ($(UNAME), $(filter $(UNAME), MINGW32_NT-5.1))
CXXFLAGS += -std=gnu++11 
CPPFLAGS += -D_WIN32 -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -U__STRICT_ANSI__
EXESUFFIX := .exe
else
ifeq (MINGW32_NT, $(findstring MINGW32_NT, $(UNAME)))
CXXFLAGS += -std=gnu++11 
CPPFLAGS += -D_WIN32 -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -D__MSVCRT_VERSION__=0x0601 -U__STRICT_ANSI__
EXESUFFIX := .exe
else
$(error "unknown platform $(UNAME)")
endif
endif
endif
endif
endif
endif

VPATH = lua/src:src:$(BUILDDIR):unittest

LUA_SOURCES = \
  lapi.c lauxlib.c lbaselib.c lcode.c \
  ldblib.c ldebug.c ldo.c ldump.c \
  lfunc.c lgc.c linit.c liolib.c \
  llex.c lmathlib.c lmem.c loadlib.c \
  lobject.c lopcodes.c loslib.c lparser.c \
  lstate.c lstring.c lstrlib.c ltable.c \
  ltablib.c ltm.c lundump.c lvm.c \
  lzio.c

LIBTUNDRA_SOURCES = \
	BinaryWriter.cpp BuildQueue.cpp Common.cpp DagGenerator.cpp \
	Driver.cpp FileInfo.cpp Hash.cpp HashTable.cpp \
	IncludeScanner.cpp JsonParse.cpp MemAllocHeap.cpp \
	MemAllocLinear.cpp MemoryMappedFile.cpp PathUtil.cpp \
	ScanCache.cpp Scanner.cpp SignalHandler.cpp StatCache.cpp \
	TargetSelect.cpp Thread.cpp dlmalloc.c TerminalIo.cpp \
	ExecUnix.cpp ExecWin32.cpp DigestCache.cpp FileSign.cpp \
	HashSha1.cpp HashFast.cpp ConditionVar.cpp ReadWriteLock.cpp

T2LUA_SOURCES = LuaMain.cpp LuaInterface.cpp LuaInterpolate.cpp LuaJsonWriter.cpp \
								LuaPath.cpp LuaProfiler.cpp

T2INSPECT_SOURCES = InspectMain.cpp

UNITTEST_SOURCES = \
	TestHarness.cpp Test_BitFuncs.cpp Test_Buffer.cpp Test_Djb2.cpp Test_Hash.cpp \
	Test_IncludeScanner.cpp Test_Json.cpp Test_MemAllocLinear.cpp Test_Pow2.cpp \
	Test_TargetSelect.cpp test_PathUtil.cpp

TUNDRA_SOURCES = Main.cpp

PATHCONTROL_SOURCES = PathControl.cpp

LUA_OBJECTS       	 := $(addprefix $(BUILDDIR)/,$(LUA_SOURCES:.c=.o))
LIBTUNDRA_OBJECTS 	 := $(addprefix $(BUILDDIR)/,$(LIBTUNDRA_SOURCES:.cpp=.o))
LIBTUNDRA_OBJECTS		 := $(LIBTUNDRA_OBJECTS:.c=.o)
T2LUA_OBJECTS     	 := $(addprefix $(BUILDDIR)/,$(T2LUA_SOURCES:.cpp=.o))
T2INSPECT_OBJECTS 	 := $(addprefix $(BUILDDIR)/,$(T2INSPECT_SOURCES:.cpp=.o))
UNITTEST_OBJECTS  	 := $(addprefix $(BUILDDIR)/,$(UNITTEST_SOURCES:.cpp=.o))
TUNDRA_OBJECTS    	 := $(addprefix $(BUILDDIR)/,$(TUNDRA_SOURCES:.cpp=.o))

ALL_SOURCES = \
							$(TUNDRA_SOURCES) \
							$(LIBTUNDRA_SOURCES) \
						 	$(LUA_SOURCES) \
							$(T2LUA_SOURCES) \
							$(T2INSPECT_SOURCES) \
							$(PATHCONTROL_SOURCES) 

ALL_DEPS    = $(ALL_SOURCES:.cpp=.d)
ALL_DEPS   := $(addprefix $(BUILDDIR)/,$(ALL_DEPS:.c=.d))

INSTALL_BASE   = $(DESTDIR)$(PREFIX)
INSTALL_BIN    = $(INSTALL_BASE)/bin
INSTALL_SCRIPT = $(INSTALL_BASE)/share/tundra

INSTALL_DIRS   = $(INSTALL_BIN) $(INSTALL_SCRIPT)
UNINSTALL_DIRS = $(INSTALL_SCRIPT)

FILES_BIN = tundra2$(EXESUFFIX) t2-lua$(EXESUFFIX) t2-inspect$(EXESUFFIX)

all: $(BUILDDIR)/tundra2$(EXESUFFIX) \
		 $(BUILDDIR)/t2-lua$(EXESUFFIX) \
		 $(BUILDDIR)/t2-inspect$(EXESUFFIX) \
		 $(BUILDDIR)/t2-unittest$(EXESUFFIX)

$(BUILDDIR)/git_version_$(GIT_BRANCH).c: $(GIT_FILE)
	sed 's/^\(.*\)/const char g_GitVersion[] = "\1";/' < $^ > $@ && \
	echo 'const char g_GitBranch[] ="'$(GIT_BRANCH)'";' >> $@

$(BUILDDIR)/git_version_$(GIT_BRANCH).o: $(BUILDDIR)/git_version_$(GIT_BRANCH).c

GIT_OBJS = $(BUILDDIR)/git_version_$(GIT_BRANCH).o

#Q ?= @
#E ?= @echo
Q ?=
E ?= @:

$(BUILDDIR):
	$(MKDIR) $(BUILDDIR)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(BUILDDIR)
	$(E) "CC $<"
	$(Q) $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(BUILDDIR)
	$(E) "CXX $<"
	$(Q) $(CXX) -c -o $@ $(CPPFLAGS) $(CXXFLAGS) $<

$(BUILDDIR)/libtundralua.a: $(LUA_OBJECTS)
	$(E) "AR $@"
	$(Q) $(AR) $@ $^

$(BUILDDIR)/libtundra.a: $(LIBTUNDRA_OBJECTS)
	$(E) "AR $@"
	$(Q) $(AR) $@ $^

$(BUILDDIR)/tundra2$(EXESUFFIX): $(TUNDRA_OBJECTS) $(BUILDDIR)/libtundra.a $(GIT_OBJS)
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(GIT_OBJS) $(TUNDRA_OBJECTS) $(LDFLAGS)

$(BUILDDIR)/t2-lua$(EXESUFFIX): $(T2LUA_OBJECTS) $(BUILDDIR)/libtundra.a $(BUILDDIR)/libtundralua.a
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(T2LUA_OBJECTS) $(LDFLAGS) -ltundralua

$(BUILDDIR)/t2-inspect$(EXESUFFIX): $(T2INSPECT_OBJECTS) $(BUILDDIR)/libtundra.a
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(T2INSPECT_OBJECTS) $(LDFLAGS)

$(BUILDDIR)/t2-unittest$(EXESUFFIX): $(UNITTEST_OBJECTS) $(BUILDDIR)/libtundra.a
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(UNITTEST_OBJECTS) $(LDFLAGS)

$(BUILDDIR)/PathControl$(EXESUFFIX): PathControl.cpp
	@mkdir -p $(BUILDDIR)
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) $(CXXLIBFLAGS) -DUNICODE $^

install:
	@echo "Installing Tundra2 to $(INSTALL_BASE)"
	$(MKDIR) $(INSTALL_DIRS)
	cd $(BUILDDIR) && $(INSTALL_X) $(FILES_BIN) $(INSTALL_BIN)
	cp -r scripts/* $(INSTALL_SCRIPT)
	@echo "Installation complete"

uninstall:
	@echo "Uninstalling Tundra2 from $(INSTALL_BASE)"
	$(RM) -r $(UNINSTALL_DIRS)
	for file in $(FILES_BIN); do \
	  $(RM) $(INSTALL_BIN)/$$file; \
	  done
	@echo "Uninstallation complete"

clean:
	$(RM) -r $(BUILDDIR)

$(BUILDDIR)/tundra-manual.html: doc/manual.asciidoc
	asciidoc -o $@ $^

ALL_SCRIPTS := $(shell find scripts -name \*.lua -or -name \*.xml)

INSTALL_PRODUCTS = \
	$(BUILDDIR)/tundra2$(EXESUFFIX) \
	$(BUILDDIR)/t2-inspect$(EXESUFFIX) \
	$(BUILDDIR)/t2-lua$(EXESUFFIX) \
	$(BUILDDIR)/PathControl$(EXESUFFIX) \
	$(BUILDDIR)/tundra-manual.html \
	$(ALL_SCRIPTS)

installer: $(BUILDDIR)/Tundra-Setup.exe
windows-zip: $(BUILDDIR)/Tundra-Binaries.zip

$(BUILDDIR)/Tundra-Binaries.zip: $(INSTALL_PRODUCTS)
	mkdir $(BUILDDIR)/__zip && \
	mkdir $(BUILDDIR)/__zip/tundra && \
	mkdir $(BUILDDIR)/__zip/tundra/bin && \
	mkdir $(BUILDDIR)/__zip/tundra/doc && \
	ln -s $$PWD/$(BUILDDIR)/tundra2$(EXESUFFIX) $(BUILDDIR)/__zip/tundra/bin/ && \
	ln -s $$PWD/$(BUILDDIR)/t2-inspect$(EXESUFFIX) $(BUILDDIR)/__zip/tundra/bin/ && \
	ln -s $$PWD/$(BUILDDIR)/t2-lua$(EXESUFFIX) $(BUILDDIR)/__zip/tundra/bin/ && \
	ln -s $$PWD/$(BUILDDIR)/tundra-manual.html $(BUILDDIR)/__zip/tundra/doc/ && \
	ln -s $$PWD/scripts $(BUILDDIR)/__zip/tundra/ && \
	(cd $(BUILDDIR)/__zip && zip -r -9 ../Tundra-Binaries.zip tundra) && \
	rm -rf $(BUILDDIR)/__zip


$(BUILDDIR)/Tundra-Setup.exe: \
	$(INSTALL_PRODUCTS) \
	windows-installer/tundra.nsi
	makensis -NOCD -DBUILDDIR=$(BUILDDIR) windows-installer/tundra.nsi > $(BUILDDIR)/nsis.log 2>&1

.PHONY: clean all install uninstall installer win-zip

-include $(ALL_DEPS)
