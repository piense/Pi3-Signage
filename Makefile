OBJS=pijpegdecoder.o main.o piresizer.o tricks.o textoverlay.o
BIN=hello_font.bin

LDFLAGS+=-lvgfont -lfreetype -lz -lilclient

include ./Makefile.include

