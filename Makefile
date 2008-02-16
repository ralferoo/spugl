############################################################################
#
# SPU 3d rasterisation library
#
# (c) 2008 Ranulf Doswell <dev@ranulf.net> 
#
# This library may not be used or distributed without a licence, please
# contact me for information if you wish to use it.
#
############################################################################

BASE_NAME = spugl-client-0.1

TARGETS = test

LIBDIRS = -L/usr/lib
LIBS = -lm -lspe -lpthread
#LIBS = -lm -lc -lspe -lpthread

USERLAND = 32
#USERLAND = 64

PPUCC = gcc
PPUCCFLAGS = -c -ggdb -m$(USERLAND) -DUSERLAND_$(USERLAND)_BITS -I. -Wno-trigraphs

SPUCC = spu-gcc -DUSERLAND_$(USERLAND)_BITS
SPUCCFLAGS = -O6 -I. -DSPU_REGS

TEXTURES_C := $(wildcard textures/*.c)
TEXTURES := $(patsubst %.c,%.o,$(TEXTURES_C))
SPU_HNDL = spu_3d.handle.O
SPU_HNDL_BASE = $(patsubst %.O,%.spe,$(SPU_HNDL))

SHARED_HEADERS = struct.h fifo.h types.h GL/*.h
PPU_OBJS = ppufifo.o glfifo.o framebuffer.o textureprep.o
SPU_OBJS = spufifo.0 decode.0 primitives.0 fragment.0 shader.0 queue.0 blocks.0
GENSOURCES = decode.c fragment.c

PPU_TEST_OBJS = $(PPU_OBJS) test.o $(TEXTURES)
PPU_SRCS := $(patsubst %.o,%.c,$(PPU_TEST_OBJS))

SOURCE_DIST_FILES= README $(PPU_SRCS) $(SPU_HNDL_BASE) $(SHARED_HEADERS) gen_spu_command_defs.h

all:	$(TARGETS)

test:	$(PPU_TEST_OBJS) $(SPU_HNDL)
	gcc -m$(USERLAND) -o test $(PPU_TEST_OBJS) $(SPU_HNDL) $(LIBS)

test.static:	$(PPU_TEST_OBJS) $(SPU_HNDL)
	gcc -m$(USERLAND) -o test.static $(PPU_TEST_OBJS) $(SPU_HNDL) $(LIBS) -static

run:	test
	./test

### BUILD-ONLY-START ###
#
# This stuff is excluded from the distribution Makefile...
#

SPU_SRCS := $(patsubst %.0,%.c,$(SPU_OBJS))

dist:	$(BASE_NAME).tar.gz

$(BASE_NAME).tar.gz:	$(SOURCE_DIST_FILES) Makefile
	rm -rf .dist
	mkdir -p .dist/$(BASE_NAME)
	sed -e '/BUILD-ONLY-''START/,/BUILD-ONLY-''END/d' <Makefile | sed -e '/DO NOT'' DELETE/,$$d' >.dist/$(BASE_NAME)/Makefile
	touch .dist/$(BASE_NAME)/spuregs.h
	tar cf - $(SOURCE_DIST_FILES) | (cd .dist/$(BASE_NAME) ; tar xf -)
	tar cfz $@ -C .dist .

edit:
	gvim -p Makefile shader.c blocks.c queue.h primitives.c test.c struct.h

source:
	make shader.s && less shader.s

GENPRODUCTS = gen_spu_command_defs.h gen_spu_command_exts.h gen_spu_command_table.h

shader.s: queue.h
#ppufifo.o: Makefile .gen
spufifo.0: Makefile $(GENPRODUCTS)

gen_spu_command_defs.h: .gen
gen_spu_command_exts.h: .gen
gen_spu_command_table.h: .gen
.gen: $(GENSOURCES) importdefs.pl
	perl importdefs.pl $(GENSOURCES)
	@touch .gen

%.handle.spe: $(SPU_OBJS) Makefile
	$(SPUCC) $(SPU_OBJS) -o $*.handle.spe
	spu-strip $*.handle.spe

depend: .gen
	@echo checking dependencies
	@makedepend -I/usr/include/python2.4/ -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(PPU_SRCS) -DUSERLAND_$(USERLAND)_BITS
	@makedepend -a -I/usr/lib/gcc/spu/4.0.2/include/ -I. -o.0 $(SPU_SRCS) -DSPU_REGS -DUSERLAND_$(USERLAND)_BITS
	@rm -f Makefile.bak
	@for i in $(SPU_OBJS) ; do grep $$i:.*spuregs.h Makefile >/dev/null || (echo ERROR: $$i does not refer to spuregs.h && false) ; done

# it's probably just me, but i never manage to get .svnignore to work...
status:
	svn status|grep -v swp$$|grep -v [0o]$$ |grep -v \\.gen

# SPU rules

%.s: %.c
	$(SPUCC) $(SPUCCFLAGS) -c -S $< -o $*.s

%.0: %.c
	$(SPUCC) $(SPUCCFLAGS) -c $< -o $*.0

%.0: %.s
	$(SPUCC) $(SPUCCFLAGS) -c $< -o $*.0

### BUILD-ONLY-END ###

###############################################################################
#
# useful targets

clean:
	rm -f *.o
#	rm -f *.spe *.O
	rm -f *.0
	rm -rf build dist
	rm -f .gen test
	rm -f textures/*.o

# gen_spu_command_defs.h gen_spu_command_exts.h gen_spu_command_table.h

###############################################################################
#
# rules

%.o: %.c
	$(PPUCC) $(PPUCCFLAGS) $< -o $@

%.o: %.cpp
	$(PPUCC) $(PPUCCFLAGS) $< -o $@

%.handle.O: %.handle.spe
	embedspu $*_handle $*.handle.spe $*.handle.O

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
ppufifo.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
ppufifo.o: /usr/include/libspe.h fifo.h types.h gen_spu_command_defs.h
glfifo.o: /usr/include/stdlib.h /usr/include/features.h
glfifo.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
glfifo.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
glfifo.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/sys/types.h
glfifo.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
glfifo.o: /usr/include/time.h /usr/include/endian.h
glfifo.o: /usr/include/bits/endian.h /usr/include/sys/select.h
glfifo.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
glfifo.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
glfifo.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
glfifo.o: /usr/include/bits/stdlib-ldbl.h /usr/include/stdio.h
glfifo.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
glfifo.o: /usr/include/bits/wchar.h /usr/include/gconv.h
glfifo.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
glfifo.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
glfifo.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
glfifo.o: fifo.h types.h gen_spu_command_defs.h struct.h spuregs.h ./GL/gl.h
glfifo.o: ./GL/glext.h /usr/include/inttypes.h /usr/include/stdint.h
framebuffer.o: /usr/include/fcntl.h /usr/include/features.h
framebuffer.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
framebuffer.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
framebuffer.o: /usr/include/bits/fcntl.h /usr/include/sys/types.h
framebuffer.o: /usr/include/bits/types.h
framebuffer.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h
framebuffer.o: /usr/include/bits/typesizes.h /usr/include/time.h
framebuffer.o: /usr/include/endian.h /usr/include/bits/endian.h
framebuffer.o: /usr/include/sys/select.h /usr/include/bits/select.h
framebuffer.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
framebuffer.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
framebuffer.o: /usr/include/stdint.h /usr/include/bits/wchar.h
framebuffer.o: /usr/include/sys/ioctl.h /usr/include/bits/ioctls.h
framebuffer.o: /usr/include/asm/ioctls.h /usr/include/asm/ioctl.h
framebuffer.o: /usr/include/bits/ioctl-types.h /usr/include/termios.h
framebuffer.o: /usr/include/bits/termios.h /usr/include/sys/ttydefaults.h
framebuffer.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
framebuffer.o: /usr/include/linux/kd.h /usr/include/linux/types.h
framebuffer.o: /usr/include/linux/posix_types.h /usr/include/linux/stddef.h
framebuffer.o: /usr/include/linux/compiler.h /usr/include/asm/posix_types.h
framebuffer.o: /usr/include/asm/types.h /usr/include/linux/tiocl.h
framebuffer.o: /usr/include/sys/time.h /usr/include/linux/fb.h
framebuffer.o: /usr/include/linux/i2c.h /usr/include/asm/ps3fb.h
framebuffer.o: /usr/include/linux/ioctl.h /usr/include/stdlib.h
framebuffer.o: /usr/include/alloca.h /usr/include/bits/stdlib-ldbl.h
framebuffer.o: /usr/include/stdio.h /usr/include/libio.h
framebuffer.o: /usr/include/_G_config.h /usr/include/wchar.h
framebuffer.o: /usr/include/gconv.h /usr/lib/gcc/spu/4.0.2/include/stdarg.h
framebuffer.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
framebuffer.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
framebuffer.o: /usr/include/string.h /usr/include/net/if.h
framebuffer.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
framebuffer.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
framebuffer.o: /usr/lib/gcc/spu/4.0.2/include/limits.h
framebuffer.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
framebuffer.o: /usr/include/asm/sockios.h /usr/include/arpa/inet.h
framebuffer.o: /usr/include/netinet/in.h /usr/include/bits/in.h
framebuffer.o: /usr/include/bits/byteswap.h fifo.h types.h
framebuffer.o: gen_spu_command_defs.h struct.h spuregs.h
textureprep.o: /usr/include/stdlib.h /usr/include/features.h
textureprep.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
textureprep.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
textureprep.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h
textureprep.o: /usr/include/sys/types.h /usr/include/bits/types.h
textureprep.o: /usr/include/bits/typesizes.h /usr/include/time.h
textureprep.o: /usr/include/endian.h /usr/include/bits/endian.h
textureprep.o: /usr/include/sys/select.h /usr/include/bits/select.h
textureprep.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
textureprep.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
textureprep.o: /usr/include/alloca.h /usr/include/bits/stdlib-ldbl.h
textureprep.o: /usr/include/stdio.h /usr/include/libio.h
textureprep.o: /usr/include/_G_config.h /usr/include/wchar.h
textureprep.o: /usr/include/bits/wchar.h /usr/include/gconv.h
textureprep.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
textureprep.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
textureprep.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
textureprep.o: types.h
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
test.o: ./GL/gl.h ./GL/glext.h /usr/include/inttypes.h /usr/include/stdint.h
test.o: ./GL/glspu.h

spufifo.0: spuregs.h struct.h types.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h fifo.h
spufifo.0: gen_spu_command_defs.h queue.h gen_spu_command_exts.h
spufifo.0: gen_spu_command_table.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
decode.0: gen_spu_command_defs.h struct.h spuregs.h primitives.h ./GL/gl.h
decode.0: ./GL/glext.h /usr/lib/gcc/spu/4.0.2/include/stddef.h
decode.0: /usr/include/inttypes.h /usr/include/features.h
decode.0: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
decode.0: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
decode.0: /usr/include/stdint.h /usr/include/bits/wchar.h
primitives.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
primitives.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
primitives.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
primitives.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
triangleColourSpan.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
triangleColourSpan.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
triangleColourSpan.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h struct.h
triangleColourSpan.0: types.h spuregs.h
fragment.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
fragment.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
fragment.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
fragment.0: gen_spu_command_defs.h struct.h spuregs.h primitives.h ./GL/gl.h
fragment.0: ./GL/glext.h /usr/lib/gcc/spu/4.0.2/include/stddef.h
fragment.0: /usr/include/inttypes.h /usr/include/features.h
fragment.0: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
fragment.0: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
fragment.0: /usr/include/stdint.h /usr/include/bits/wchar.h
shader.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
shader.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
shader.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
shader.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
queue.0: spuregs.h struct.h types.h
queue.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
queue.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h queue.h
queue.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
blocks.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
blocks.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
blocks.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
blocks.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
