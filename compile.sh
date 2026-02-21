#!/bin/bash

gcc -o xsrend \
	src/main.c src/camera.c src/text.c\
	lib/libSDL2.a \
	-g -fsanitize=address \
	-lm \
	-Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith \
	-Wcast-align -Wstrict-prototypes -Wwrite-strings \
	-Wswitch-default -Winit-self -Wold-style-definition \
	-Wno-format-truncation -Wformat \
	-march=native -O3\
