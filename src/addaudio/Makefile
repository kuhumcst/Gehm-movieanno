# Makefile for addaudio
#
#   make            build ./addaudio
#   make run        build and run it
#   make debug      build with -O0 -g and no NDEBUG
#   make clean      remove objects and the executable
#
# addaudio calls ffmpeg, but does not link against it: only a C++20 compiler
# with <filesystem> is needed. On Windows build it with MinGW ("mingw32-make")
# or from a Visual Studio prompt with
#
#   cl /std:c++20 /EHsc /Fe:addaudio.exe addaudio.cpp

TARGET   := addaudio
SRCS     := addaudio.cpp
BUILDDIR := build

CXX      ?= g++

WARNINGS := -Wall -Wextra -Wno-unused-parameter
OPTIM    ?= -O2 -DNDEBUG

CXXFLAGS ?= $(OPTIM)
CXXFLAGS += -std=c++20 $(WARNINGS)
CPPFLAGS += -MMD -MP

# GCC before 9 keeps std::filesystem in a library of its own.
LDLIBS   += $(shell $(CXX) -dumpversion 2>/dev/null | awk -F. '$$1 < 9 {print "-lstdc++fs"}')

OBJS := $(addprefix $(BUILDDIR)/,$(SRCS:.cpp=.o))
DEPS := $(OBJS:.o=.d)

.PHONY: all run debug clean help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

run: $(TARGET)
	./$(TARGET) $(ARGS)

debug:
	$(MAKE) OPTIM="-O0 -g" all

clean:
	rm -rf $(BUILDDIR) $(TARGET)

help:
	@echo "targets: all (default), run [ARGS=...], debug, clean"
	@echo "vars:    CXX, CXXFLAGS, OPTIM"
