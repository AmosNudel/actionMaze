#**************************************************************************************************
#
#   Dungeon Foray - makefile for desktop platforms (Windows / Linux / macOS)
#
#   Compiles every .cpp found under $(SRC_DIR) recursively, so new source files
#   need no makefile change. Object files and generated header dependencies land
#   in $(OBJ_DIR), mirroring the source tree.
#
#   Usage:
#     make                      release build
#     make BUILD_MODE=DEBUG     debug build (-g -O0)
#     make package              build and stage an itch.io Windows ZIP in dist/
#     make clean                remove objects and the executable
#     make run                  build, then launch
#
#   The executable's icon is generated rather than checked in by hand:
#     python tools/make_icon.py   redraw assets/icon/ from the menu backdrop
#
#   Based on the raylib makefile, Copyright (c) 2013-2019 Ramon Santamaria (@raysan5)
#
#**************************************************************************************************

.PHONY: all clean package run

PROJECT_NAME  ?= DungeonForay
BUILD_MODE    ?= RELEASE
PLATFORM      ?= PLATFORM_DESKTOP

SRC_DIR       ?= src
OBJ_DIR       ?= obj

# Where raylib and the toolchain live (Windows defaults match the raylib installer)
RAYLIB_PATH   ?= C:/raylib/raylib
COMPILER_PATH ?= C:/raylib/w64devkit/bin

# Prefix raylib was installed under on Linux/macOS (headers in include/, libs in lib/)
DESTDIR       ?= /usr/local

# Determine host platform
ifeq ($(OS),Windows_NT)
    PLATFORM_OS = WINDOWS
    export PATH := $(COMPILER_PATH):$(PATH)
    # Use the toolchain shell so recipes below can rely on POSIX commands
    ifneq ($(wildcard $(COMPILER_PATH)/sh.exe),)
        SHELL := $(COMPILER_PATH)/sh.exe
        .SHELLFLAGS := -c
    endif
else
    UNAMEOS = $(shell uname)
    ifeq ($(UNAMEOS),Darwin)
        PLATFORM_OS = OSX
    else
        PLATFORM_OS = LINUX
    endif
endif

CXX = g++
ifeq ($(PLATFORM_OS),OSX)
    CXX = clang++
endif

CXXFLAGS = -std=c++14 -Wall -Wno-missing-braces -MMD -MP

ifeq ($(BUILD_MODE),DEBUG)
    CXXFLAGS += -g -O0
else
    CXXFLAGS += -O2
endif

INCLUDE_PATHS = -I$(SRC_DIR)
# lib/ comes first on purpose. It holds a raylib built with SUPPORT_GPU_SKINNING,
# which the animated enemies need: without it raylib skins on the CPU, into the
# model's own vertex buffers, and every enemy sharing one Model shares one pose.
# The install under $(RAYLIB_PATH) is stock, so the other projects on it are
# unaffected. To rebuild lib/libraylib.a:
#   make -C $(RAYLIB_PATH)/src CUSTOM_CFLAGS="-DSUPPORT_GPU_SKINNING=1"
# then copy $(RAYLIB_PATH)/src/libraylib.a here and restore that tree's own copy.
# Delete lib/libraylib.a to fall back to the shared install; enemies then need
# CPU skinning, i.e. a Model per enemy.
LDFLAGS = -L. -Llib
EXT =

ifeq ($(PLATFORM_OS),WINDOWS)
    INCLUDE_PATHS += -I$(RAYLIB_PATH)/src -I$(RAYLIB_PATH)/src/external
    LDFLAGS += -L$(RAYLIB_PATH)/src
    # NOTE: WinMM library required to set high-res timer resolution
    LDLIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
    EXT = .exe
    # The executable's icon and its Properties fields - see $(PROJECT_NAME).rc.
    # This is what replaced raylib.rc.data out of the raylib source tree: that put
    # raylib's own icon on the .exe, which is the wrong badge for a finished game.
    RES = $(OBJ_DIR)/$(PROJECT_NAME).res
    ifneq ($(BUILD_MODE),DEBUG)
        LDFLAGS += -Wl,--subsystem,windows
    endif
endif

ifeq ($(PLATFORM_OS),LINUX)
    INCLUDE_PATHS += -I$(DESTDIR)/include
    LDFLAGS += -L$(DESTDIR)/lib
    LDLIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

ifeq ($(PLATFORM_OS),OSX)
    INCLUDE_PATHS += -I$(DESTDIR)/include
    LDFLAGS += -L$(DESTDIR)/lib
    ifneq ($(wildcard /opt/homebrew/include/.*),)
        INCLUDE_PATHS += -I/opt/homebrew/include
        LDFLAGS += -L/opt/homebrew/lib
    endif
    LDLIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo
endif

# Recursive wildcard: $(call rwildcard,<dir>,<pattern>)
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SOURCES = $(call rwildcard,$(SRC_DIR),*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS = $(OBJS:.o=.d)

TARGET = $(PROJECT_NAME)$(EXT)
DIST_DIR ?= dist
PACKAGE_DIR = $(DIST_DIR)/$(PROJECT_NAME)-windows
PACKAGE_ZIP = $(DIST_DIR)/$(PROJECT_NAME)-windows.zip

all: $(TARGET)

$(TARGET): $(OBJS) $(RES)
	$(CXX) -o $@ $(OBJS) $(RES) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

# Windows only - RES is empty everywhere else, so this rule is never asked for.
# Depends on the .ico as well as the .rc: regenerating the icon has to relink,
# and windres will not tell us it did not.
$(OBJ_DIR)/$(PROJECT_NAME).res: $(PROJECT_NAME).rc assets/icon/dungeon_foray.ico
	@mkdir -p $(dir $@)
	windres $< -O coff -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

run: $(TARGET)
	./$(TARGET)

# itch.io accepts a runnable ZIP directly. The archive contains only the release
# executable, every runtime asset, and the credits; source and development files
# stay out of the upload. This target is Windows-only because package.ps1 uses
# PowerShell's built-in Compress-Archive. Object files are shared by both build
# modes, so clean first: otherwise a prior debug build can be packaged unchanged.
package:
	@if [ "$(PLATFORM_OS)" != "WINDOWS" ]; then echo "package is supported on Windows only"; exit 1; fi
	$(MAKE) clean
	$(MAKE) BUILD_MODE=RELEASE all
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/package.ps1 \
		-ProjectName "$(PROJECT_NAME)" -Executable "$(TARGET)" \
		-PackageDir "$(PACKAGE_DIR)" -PackageZip "$(PACKAGE_ZIP)"

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(TARGET)
	@echo Cleaning done

# Header dependencies generated by -MMD, so edits to .h files rebuild what uses them
-include $(DEPS)
