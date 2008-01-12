############################################################################
#
# SPU 3d rasterisation library
#
# (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
#
############################################################################

TARGETS = test

LIBDIRS = -L/usr/lib
LIBS = -lstdc++ -lm -lc -lspe -lpthread

USERLAND = 32
#USERLAND = 64

PPUCC = gcc
PPUCCFLAGS = -c -ggdb -m$(USERLAND) -DUSERLAND_$(USERLAND)_BITS -I.

SPUCC = spu-gcc -DUSERLAND_$(USERLAND)_BITS
SPUCCFLAGS = -O6

SPU_OBJS = spufifo.spe.o
SPU_HNDL = spu_3d.handle.o
PPU_OBJS = ppufifo.o glimp.o
PPU_TEST_OBJS = $(PPU_OBJS) test.o

PPU_SRCS := $(patsubst %.o,%.c,$(PPU_TEST_OBJS))
SPU_SRCS := $(patsubst %.spe.o,%.c,$(SPU_OBJS))

all:	$(TARGETS)

test:	$(PPU_TEST_OBJS) $(SPU_HNDL)
	gcc -m$(USERLAND) -o test $(PPU_TEST_OBJS) $(SPU_HNDL) $(LIBS)

run:	test
	./test

ppufifo.o: Makefile
spufifo.spe.o: Makefile

###############################################################################
#
# useful targets

depend:
	makedepend -I/usr/include/python2.4/ -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(PPU_SRCS) -DUSERLAND_$(USERLAND)_BITS
	makedepend -a -I/usr/lib/gcc/spu/4.0.2/include/ -o.spe.o $(SPU_SRCS) -DUSERLAND_$(USERLAND)_BITS

clean:
	rm -f *.o
	rm -f *.spe *.spe.o
	rm -rf build dist

###############################################################################
#
# rules

%.o: %.c
	$(PPUCC) $(PPUCCFLAGS) $< -o $@

%.o: %.cpp
	$(PPUCC) $(PPUCCFLAGS) $< -o $@

%.s: %.c
	$(SPUCC) $(SPUCCFLAGS) -c -S $< -o $*.s

%.spe.o: %.c
	$(SPUCC) $(SPUCCFLAGS) -c $< -o $*.spe.o

%.spe.o: %.s
	$(SPUCC) $(SPUCCFLAGS) -c $< -o $*.spe.o

%.handle.o: $(SPU_OBJS) Makefile
	$(SPUCC) $(SPU_OBJS) -o $*.handle.spe
	spu-strip $*.handle.spe
	embedspu $*_handle $*.handle.spe $*.handle.o

###############################################################################
#
# use "make depend" to update this
#
# DO NOT DELETE

ppufifo.o: /usr/include/stdlib.h /usr/include/features.h
ppufifo.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ppufifo.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ppufifo.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/sys/types.h
ppufifo.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ppufifo.o: /usr/include/time.h /usr/include/endian.h
ppufifo.o: /usr/include/bits/endian.h /usr/include/sys/select.h
ppufifo.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ppufifo.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
ppufifo.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
ppufifo.o: /usr/include/bits/stdlib-ldbl.h /usr/include/stdio.h
ppufifo.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
ppufifo.o: /usr/include/bits/wchar.h /usr/include/gconv.h
ppufifo.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
ppufifo.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
ppufifo.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
ppufifo.o: /usr/include/libspe.h 3d.h
glimp.o: /usr/include/stdlib.h /usr/include/features.h
glimp.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
glimp.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
glimp.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/sys/types.h
glimp.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
glimp.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
glimp.o: /usr/include/sys/select.h /usr/include/bits/select.h
glimp.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
glimp.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
glimp.o: /usr/include/alloca.h /usr/include/bits/stdlib-ldbl.h
glimp.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
glimp.o: /usr/include/wchar.h /usr/include/bits/wchar.h /usr/include/gconv.h
glimp.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
glimp.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
glimp.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h 3d.h
glimp.o: ./GLES/gl.h ./GLES/glplatform.h
test.o: /usr/include/stdlib.h /usr/include/features.h
test.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
test.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
test.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/sys/types.h
test.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
test.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
test.o: /usr/include/sys/select.h /usr/include/bits/select.h
test.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
test.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
test.o: /usr/include/alloca.h /usr/include/bits/stdlib-ldbl.h
test.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
test.o: /usr/include/wchar.h /usr/include/bits/wchar.h /usr/include/gconv.h
test.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
test.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
test.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
test.o: ./GLES/gl.h ./GLES/glplatform.h ./GLES/glspu.h

spufifo.spe.o: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
spufifo.spe.o: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
spufifo.spe.o: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h 3d.h
