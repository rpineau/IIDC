# Makefile for libIIDC

CC = gcc
CFLAGS = -fpic -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
CPPFLAGS = -fpic -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
LDFLAGS = -shared -lstdc++
RM = rm -f
STRIP = strip
TARGET_LIB = libIIDC.so

SRCS = main.cpp cameraIIDC.cpp x2camera.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^ static_libs/`arch`/libdc1394.a static_libs/`arch`/libusb-1.0.a
	$(STRIP) $@ >/dev/null 2>&1  || true

$(SRCS:.cpp=.d):%.d:%.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -MM $< >$@

include $(SRCS:.cpp=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.cpp=.d)
