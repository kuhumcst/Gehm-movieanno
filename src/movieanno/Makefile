# Makefile for movieanno
#
#   make            build ./movieanno
#   make run        build and run it
#   make debug      build with -O0 -g and no NDEBUG
#   make clean      remove objects and the executable
#
# OpenCV flags come from pkg-config (package "opencv4"). If your OpenCV is
# installed somewhere pkg-config does not know about, either set
# PKG_CONFIG_PATH to the directory holding opencv4.pc, or override the flags
# directly, e.g.:
#
#   make OPENCV_CFLAGS=-I/opt/opencv/include/opencv4 \
#        OPENCV_LIBS="-L/opt/opencv/lib -lopencv_core -lopencv_highgui ..."

TARGET   := movieanno
SRCS     := movieanno.cpp argopt.cpp option.cpp
BUILDDIR := build

CXX      ?= g++
PKG_CONFIG ?= pkg-config

OPENCV_PKG    := opencv4
OPENCV_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags $(OPENCV_PKG) 2>/dev/null)
OPENCV_LIBS   ?= $(shell $(PKG_CONFIG) --libs   $(OPENCV_PKG) 2>/dev/null)

WARNINGS := -Wall -Wextra -Wno-unused-parameter
OPTIM    ?= -O2 -DNDEBUG

CXXFLAGS ?= $(OPTIM)
CXXFLAGS += -std=c++20 $(WARNINGS) $(OPENCV_CFLAGS)
CPPFLAGS += -MMD -MP
LDLIBS   += $(OPENCV_LIBS)

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

# Fail early with a useful message instead of a wall of "opencv2/opencv.hpp: No such file".
ifeq ($(strip $(OPENCV_CFLAGS)$(OPENCV_LIBS)),)
$(OBJS): check-opencv
$(TARGET): check-opencv
.PHONY: check-opencv
check-opencv:
	@echo "error: pkg-config could not find '$(OPENCV_PKG)'." >&2
	@echo "       Install OpenCV development files (e.g. 'sudo apt install libopencv-dev')," >&2
	@echo "       or set PKG_CONFIG_PATH / OPENCV_CFLAGS / OPENCV_LIBS. See 'make help'." >&2
	@false
endif

run: $(TARGET)
	./$(TARGET) $(ARGS)

debug:
	$(MAKE) OPTIM="-O0 -g" all

clean:
	rm -rf $(BUILDDIR) $(TARGET)

help:
	@echo "targets: all (default), run [ARGS=...], debug, clean"
	@echo "vars:    CXX, CXXFLAGS, OPTIM, OPENCV_CFLAGS, OPENCV_LIBS, PKG_CONFIG_PATH"

-include $(DEPS)
