BR_DIR = ../buildroot-2016.11
CC=${BR_DIR}/output/host/usr/bin/arm-linux-cc

OBJS=main.o

OBJS+= compositor/ilclient/ilclient.o compositor/ilclient/ilcore.o
LIB+= compositor/ilclient/libilclient.a

OBJS+= compositor/vgfont/font.o compositor/vgfont/vgft.o compositor/vgfont/graphics.o
LIBS+= compositor/vgfont/libvgfont.a

OBJS+= compositor/compositor.o compositor/pijpegdecoder.o compositor/piresizer.o compositor/tricks.o compositor/textoverlay.o

BIN=hello_font.bin

CFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi

LDFLAGS+= -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lm -lvcilcs -lvchostif -lfreetype

INCLUDES+= -I./ -I${BR_DIR}/output/staging/usr/include/freetype2/

all: $(BIN) $(LIB)

%.o: %.c
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

%.o: %.cpp
	@rm -f $@ 
	$(CXX) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

%.bin: $(OBJS)
	$(CC) -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

%.a: $(OBJS)
	$(AR) r $@ $^

clean:
	for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f $(BIN) $(LIB)



