BR_DIR = ../BR-Pi2
CC=${BR_DIR}/output/host/usr/bin/arm-linux-cc
CXX=${BR_DIR}/output/host/usr/bin/arm-linux-g++

OBJS=main.o

OBJS+= compositor/ilclient/ilclient.o compositor/ilclient/ilcore.o 
LIB+= compositor/ilclient/libilclient.a

OBJS+= compositor/vgfont/font.o compositor/vgfont/vgft.o compositor/vgfont/graphics.o compositor/tricks.o compositor/piSlideRenderer.o
LIBS+= compositor/vgfont/libvgfont.a 

OBJS+=  compositor/fontTest.o compositor/PiSlideShow.o compositor/pijpegdecoder.o compositor/piImageResizer.o
OBJS += graphicsTests.o PiSignageLogging.o PiSlide.o

BIN=PiSignage.bin

CFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi

LDFLAGS+= -lbrcmGLESv2 -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread
LDFLAGS+= -lrt -lm -lvcilcs -lvchostif -lfreetype -lcairo -lpangoft2-1.0 -lpangocairo-1.0 -lpango-1.0 -lglib-2.0 -lgobject-2.0

INCLUDES+= -I./ -I${BR_DIR}/output/staging/usr/include/freetype2/
INCLUDES+= -I${BR_DIR}/output/staging/usr/include/freetype2/
INCLUDES+= -I${BR_DIR}/output/staging/usr/include/cairo -I${BR_DIR}/output/staging/usr/include/pango-1.0
INCLUDES+= -I${BR_DIR}/output/staging/usr/include/glib-2.0
INCLUDES+= -I${BR_DIR}/output/staging/usr/lib/glib-2.0/include

all: $(BIN) $(LIB)

%.o: %.c
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

%.o: %.cc
	@rm -f $@ 
	$(CXX) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

%.bin: $(OBJS)
	$(CXX) -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

%.a: $(OBJS)
	$(AR) r $@ $^

clean:
	for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f $(BIN) $(LIB)




