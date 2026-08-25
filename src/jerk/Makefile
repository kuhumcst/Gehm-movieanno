# Makefile for jerk
#
#   make            build ./jerk
#   make run        build and run it
#   make debug      build with -O0 -g and no NDEBUG
#   make clean      remove objects and the executable
#

TARGET   := jerk
SRCS     := jerk.cpp
BUILDDIR := build

CXX      ?= g++
PKG_CONFIG ?= pkg-config

WARNINGS := -Wall -Wextra -Wno-unused-parameter
OPTIM    ?= -O2 -DNDEBUG

CXXFLAGS ?= $(OPTIM)
CXXFLAGS += -std=c++20 $(WARNINGS)
CPPFLAGS += -MMD -MP

OBJS := $(addprefix $(BUILDDIR)/,$(SRCS:.cpp=.o))
DEPS := $(OBJS:.o=.d)

.PHONY: all run debug clean help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

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

-include $(DEPS)
