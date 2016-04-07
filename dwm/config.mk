# dwm version
VERSION = 6.0

#
# paths
#
PREFIX    ?= /usr/local
MANPREFIX ?= ${PREFIX}/share/man

#
# flags
#
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS   = `pkg-config --cflags x11 xinerama cairo` -DXINERAMA -Wall
LDFLAGS  = `pkg-config --libs x11 xinerama cairo`

#
# compiler and linker
#
CC       = cc
