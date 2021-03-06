# -*- mode: makefile -*-

LIB_SRCS:= \
	state.c \
	sega3d.c \
	list.c \
	plist.c \
	tlist.c \
	transform.c \
	sort.c \
	matrix_stack.c \
	fog.c \
	ztp.c

INSTALL_HEADER_FILES:= \
	./:sega3d.h:./sega3d/ \
	./:sega3d-types.h:./sega3d/ \
	./:sgl.h:./sega3d/ \
	./:ztp.h:./sega3d/
