CC ?= clang
CXX ?= clang++
AR= ar rcus

RM = rm -f
MKDIR = mkdir -p
INSTALL_X = install -m 0755
INSTALL_F = install -m 0644

CFLAGS ?= -Wall
CPPFLAGS = -Ilua/src -Isrc -MMD -MP
CXXFLAGS ?= $(CFLAGS) -std=c++11 -fno-exceptions

CXXLIBFLAGS ?=
LDFLAGS ?= -Lbuild -ltundra

PREFIX ?= /usr/local

CHECKED ?= no
ifeq ($(CHECKED), no)
CFLAGS += -O3 -DNDEBUG
else
CFLAGS += -D_DEBUG
endif

UNAME := $(shell uname)
ifeq ($(UNAME), $(filter $(UNAME), FreeBSD NetBSD OpenBSD))
LDFLAGS += -lpthread
else
ifeq ($(UNAME), $(filter $(UNAME), Linux))
LDFLAGS += -pthread
else
ifeq ($(UNAME), $(filter $(UNAME), Darwin))
CXXFLAGS += -stdlib=libc++
LDFLAGS += -stdlib=libc++
else
$(error "unknown platform $(UNAME)")
endif
endif
endif

VPATH = lua/src:src:build:unittest

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
	ExecUnix.cpp

T2LUA_SOURCES = LuaMain.cpp LuaInterface.cpp LuaInterpolate.cpp LuaJsonWriter.cpp

T2INSPECT_SOURCES = InspectMain.cpp

UNITTEST_SOURCES = \
	TestHarness.cpp Test_BitFuncs.cpp Test_Buffer.cpp Test_Djb2.cpp Test_Hash.cpp \
	Test_IncludeScanner.cpp Test_Json.cpp Test_MemAllocLinear.cpp Test_Pow2.cpp \
	Test_TargetSelect.cpp test_PathUtil.cpp

TUNDRA_SOURCES = Main.cpp

LUA_OBJECTS       = $(addprefix build/,$(LUA_SOURCES:.c=.o))
LIBTUNDRA_OBJECTS = $(addprefix build/,$(LIBTUNDRA_SOURCES:.cpp=.o))
LIBTUNDRA_OBJECTS:= $(LIBTUNDRA_OBJECTS:.c=.o)
T2LUA_OBJECTS     = $(addprefix build/,$(T2LUA_SOURCES:.cpp=.o))
T2INSPECT_OBJECTS = $(addprefix build/,$(T2INSPECT_SOURCES:.cpp=.o))
UNITTEST_OBJECTS  = $(addprefix build/,$(UNITTEST_SOURCES:.cpp=.o))
TUNDRA_OBJECTS    = $(addprefix build/,$(TUNDRA_SOURCES:.cpp=.o))

ALL_SOURCES = $(TUNDRA_SOURCES) $(LIBTUNDRA_SOURCES) $(LUA_SOURCES) $(T2LUA_SOURCES) $(T2INSPECT_SOURCES)
ALL_DEPS    = $(ALL_SOURCES:.cpp=.d)
ALL_DEPS   := $(addprefix build/,$(ALL_DEPS:.c=.d))

INSTALL_BASE   = $(DESTDIR)$(PREFIX)
INSTALL_BIN    = $(INSTALL_BASE)/bin
INSTALL_SCRIPT = $(INSTALL_BASE)/share/tundra

INSTALL_DIRS   = $(INSTALL_BIN) $(INSTALL_SCRIPT)
UNINSTALL_DIRS = $(INSTALL_SCRIPT)

FILES_BIN = tundra2 t2-lua t2-inspect

all: build build/tundra2 build/t2-lua build/t2-inspect build/t2-unittest

#Q ?= @
#E ?= @echo
Q ?=
E ?= @:

build:
	$(MKDIR) build

build/%.o: %.c
	$(E) "CC $<"
	$(Q) $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

build/%.o: %.cpp
	$(E) "CXX $<"
	$(Q) $(CXX) -c -o $@ $(CPPFLAGS) $(CXXFLAGS) $<

build/libtundralua.a: $(LUA_OBJECTS)
	$(E) "AR $@"
	$(Q) $(AR) $@ $^

build/libtundra.a: $(LIBTUNDRA_OBJECTS)
	$(E) "AR $@"
	$(Q) $(AR) $@ $^

build/tundra2: $(TUNDRA_OBJECTS) build/libtundra.a
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(TUNDRA_OBJECTS) $(LDFLAGS)

build/t2-lua: $(T2LUA_OBJECTS) build/libtundra.a build/libtundralua.a
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(T2LUA_OBJECTS) $(LDFLAGS) -ltundralua

build/t2-inspect: $(T2INSPECT_OBJECTS) build/libtundra.a
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(T2INSPECT_OBJECTS) $(LDFLAGS)

build/t2-unittest: $(UNITTEST_OBJECTS) build/libtundra.a
	$(E) "LINK $@"
	$(Q) $(CXX) -o $@ $(CXXLIBFLAGS) $(UNITTEST_OBJECTS) $(LDFLAGS)

install:
	@echo "Installing Tundra2 to $(INSTALL_BASE)"
	$(MKDIR) $(INSTALL_DIRS)
	cd build && $(INSTALL_X) $(FILES_BIN) $(INSTALL_BIN)
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
	$(RM) -r build

.PHONY: clean all install uninstall

-include $(ALL_DEPS)
