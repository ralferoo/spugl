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

PPUCC = gcc
PPUCCFLAGS = -c -ggdb

SPUCC = spu-gcc
SPUCCFLAGS = -O6

SPU_OBJS = spumain.spe.o
SPU_HNDL = spu_3d.handle.o
PPU_OBJS = driver.o

PPU_SRCS := $(patsubst %.o,%.c,$(PPU_OBJS))
SPU_SRCS := $(patsubst %.spe.o,%.c,$(SPU_OBJS))

all:	$(TARGETS)

test:	$(PPU_OBJS) $(SPU_HNDL)
	gcc -o test $(PPU_OBJS) $(SPU_HNDL) $(LIBS)

run:	test
	./test

###############################################################################
#
# useful targets

depend:
	makedepend -I/usr/include/python2.4/ -I/usr/lib/gcc/spu/4.0.2/include/ $(PPU_SRCS)
	makedepend -a -I/usr/lib/gcc/spu/4.0.2/include/ -o.spe.o $(SPU_SRCS)

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

driver.o: /usr/include/stdlib.h /usr/include/features.h
driver.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
driver.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
driver.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/sys/types.h
driver.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
driver.o: /usr/include/time.h /usr/include/endian.h
driver.o: /usr/include/bits/endian.h /usr/include/sys/select.h
driver.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
driver.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
driver.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
driver.o: /usr/include/bits/stdlib-ldbl.h /usr/include/stdio.h
driver.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
driver.o: /usr/include/bits/wchar.h /usr/include/gconv.h
driver.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
driver.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
driver.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
driver.o: /usr/include/libspe.h 3d.h

spumain.spe.o: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
spumain.spe.o: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
spumain.spe.o: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h 3d.h
