############################################################################
#
# SPU GL - 3d rasterisation library for the PS3
#
# (c) 2008 Ranulf Doswell <dev@ranulf.net> 
#
# This library may not be used or distributed without a licence, please
# contact me for information if you wish to use it.
#
############################################################################

BASE_NAME = spugl-client-0.1

TARGETS = test server client

LIBDIRS = -L/usr/lib

#LIBS = -lm -lspe -lpthread -lrt
#LIBSPE2 = 

LIBS = -lm -lspe2 -lpthread -lrt
LIBSPE2 = -DUSE_LIBSPE2

USERLAND = 32
#USERLAND = 64

PPUCC = gcc
PPUCCFLAGS = -c -ggdb -m$(USERLAND) $(LIBSPE2) -DUSERLAND_$(USERLAND)_BITS -I. -Wno-trigraphs -std=gnu99

NEWSPUCC = cellgcc -DUSERLAND_$(USERLAND)_BITS -std=gnu99 -I/usr/include
SPUCC = spu-gcc -DUSERLAND_$(USERLAND)_BITS -std=gnu99 -fpic
SPUCCFLAGS = -O6 -I. -DSPU_REGS

TEXTURES_C := $(wildcard textures/*.c)
TEXTURES := $(patsubst %.c,%.o,$(TEXTURES_C))
SPU_HNDL = spu_3d.handle.o$(USERLAND)
SPU_HNDL_BASE = $(patsubst %.o$(USERLAND),%.spe,$(SPU_HNDL))

SHARED_HEADERS = struct.h fifo.h types.h GL/*.h
PPU_OBJS = ppufifo.o glfifo.o framebuffer.o textureprep.o joystick.o
SPU_OBJS = spufifo.0 decode.0 primitives.0 fragment.0 queue.0 activeblock.0 shader.0 texture.0 scaler.0 oldshader.0
GENSOURCES = decode.c fragment.c

PPU_TEST_OBJS = $(PPU_OBJS) test.o $(TEXTURES)
PPU_SRCS := $(patsubst %.o,%.c,$(PPU_TEST_OBJS))

SOURCE_DIST_FILES= README $(PPU_SRCS) $(SPU_HNDL) $(SHARED_HEADERS) gen_spu_command_defs.h

DAEMON_TARGETS_C := $(wildcard spugl-server/*.c)
DAEMON_TARGETS_H := $(wildcard spugl-server/*.h)
DAEMON_TARGETS := $(patsubst %.c,%.o,$(DAEMON_TARGETS_C))

CLIENT_TARGETS = testclient.o spugl.a

CLIENT_LIB_TARGETS_C := $(wildcard spugl-client/*.c)
CLIENT_LIB_TARGETS_H := $(wildcard spugl-client/*.h)
CLIENT_LIB_TARGETS := $(patsubst %.c,%.o,$(CLIENT_LIB_TARGETS_C))

all:	$(TARGETS)

server:	$(DAEMON_TARGETS)
	gcc -m$(USERLAND) -o $@ $(DAEMON_TARGETS)

client:	$(CLIENT_TARGETS)
	gcc -m$(USERLAND) -o $@ $(CLIENT_TARGETS)

spugl.a:	$(CLIENT_LIB_TARGETS)
	ar -r $@ $(CLIENT_LIB_TARGETS)

test:	$(PPU_TEST_OBJS) $(SPU_HNDL)
	gcc -m$(USERLAND) -o test $(PPU_TEST_OBJS) $(SPU_HNDL) $(LIBDIRS) $(LIBS)

texmap.sqf:	test
	strip $<
	rm -rf /tmp/texmap-build
	mkdir /tmp/texmap-build
	cp $< /tmp/texmap-build/launch
	strip /tmp/texmap-build/launch
	echo 'Texture Mapping Demo':`date '+%s'` >/tmp/texmap-build/.version
	mksquashfs /tmp/texmap-build $@ -noappend
	
spugl-client/daemon.h: .git
	./version.sh

test.static:	$(PPU_TEST_OBJS) $(SPU_HNDL)
	gcc -m$(USERLAND) -o test.static $(PPU_TEST_OBJS) $(SPU_HNDL) $(LIBS) -static

run:	test
	./test

huge:	test
	./test -0.65

big:	test
	./test -0.9

medium:	test
	./test -1.7

small:	test
	./test -2.0

dump:	test
	./test dump.avi

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
	gvim -p shader.c texture.c queue.h test.c struct.h glfifo.c textureprep.c decode.c primitives.c

source:
	make shader.s && less shader.s

GENPRODUCTS = gen_spu_command_defs.h gen_spu_command_exts.h gen_spu_command_table.h

ppufifo.o: Makefile

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
	@makedepend -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(PPU_SRCS) -DUSERLAND_$(USERLAND)_BITS
	@makedepend -a -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(CLIENT_LIB_TARGETS_C) $(DAEMON_TARGETS_C)
	@makedepend -a -I/usr/lib/gcc/spu/4.0.2/include/ -I. -o.0 $(SPU_SRCS) -DSPU_REGS -DUSERLAND_$(USERLAND)_BITS
	@rm -f Makefile.bak
	@for i in $(SPU_OBJS) ; do grep $$i:.*spuregs.h Makefile >/dev/null || [ ! -f `basename $$i .0`.c ] || ( echo ERROR: $$i does not refer to spuregs.h && false) ; done

# SPU rules

%.s: %.c
	$(SPUCC) $(SPUCCFLAGS) -c -S $< -o $*.s

%.0: %.c
	$(SPUCC) $(SPUCCFLAGS) -c $< -o $*.0

%.0: %.s
	$(SPUCC) $(SPUCCFLAGS) -c $< -o $*.0

%.handle.o$(USERLAND): %.handle.spe
	embedspu $*_handle $*.handle.spe $*.handle.o$(USERLAND)

### BUILD-ONLY-END ###

###############################################################################
#
# useful targets

clean:
	rm -f *.o
#	rm -f *.spe *.o$(USERLAND)
	rm -f *.0
	rm -rf build dist
	rm -f .gen test
	rm -f textures/*.o
	rm -f server client spugl-*/*.o

# gen_spu_command_defs.h gen_spu_command_exts.h gen_spu_command_table.h

###############################################################################
#
# rules

%.o: %.c
	$(PPUCC) $(PPUCCFLAGS) $< -o $@

%.o: %.cpp
	$(PPUCC) $(PPUCCFLAGS) $< -o $@

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
ppufifo.o: /usr/include/sys/mman.h /usr/include/bits/mman.h fifo.h types.h
ppufifo.o: gen_spu_command_defs.h /usr/include/libspe.h
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
glfifo.o: /usr/include/math.h /usr/include/bits/huge_val.h
glfifo.o: /usr/include/bits/mathdef.h /usr/include/bits/mathcalls.h fifo.h
glfifo.o: types.h gen_spu_command_defs.h struct.h spuregs.h ./GL/gl.h
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
framebuffer.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
framebuffer.o: /usr/include/bits/confname.h /usr/include/getopt.h
framebuffer.o: /usr/include/sys/ioctl.h /usr/include/bits/ioctls.h
framebuffer.o: /usr/include/asm/ioctls.h /usr/include/asm/ioctl.h
framebuffer.o: /usr/include/bits/ioctl-types.h /usr/include/termios.h
framebuffer.o: /usr/include/bits/termios.h /usr/include/sys/ttydefaults.h
framebuffer.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
framebuffer.o: /usr/include/linux/kd.h /usr/include/linux/types.h
framebuffer.o: /usr/include/linux/posix_types.h /usr/include/linux/stddef.h
framebuffer.o: /usr/include/asm/posix_types.h /usr/include/asm/types.h
framebuffer.o: /usr/include/linux/tiocl.h /usr/include/sys/time.h
framebuffer.o: /usr/include/linux/fb.h /usr/include/linux/i2c.h
framebuffer.o: /usr/include/asm/ps3fb.h /usr/include/linux/ioctl.h
framebuffer.o: /usr/include/stdlib.h /usr/include/alloca.h
framebuffer.o: /usr/include/bits/stdlib-ldbl.h /usr/include/stdio.h
framebuffer.o: /usr/include/libio.h /usr/include/_G_config.h
framebuffer.o: /usr/include/wchar.h /usr/include/gconv.h
framebuffer.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
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
textureprep.o: struct.h types.h spuregs.h ./GL/glspu.h ./GL/gl.h ./GL/glext.h
textureprep.o: /usr/include/inttypes.h /usr/include/stdint.h
joystick.o: /usr/include/stdlib.h /usr/include/features.h
joystick.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
joystick.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
joystick.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/sys/types.h
joystick.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
joystick.o: /usr/include/time.h /usr/include/endian.h
joystick.o: /usr/include/bits/endian.h /usr/include/sys/select.h
joystick.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
joystick.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
joystick.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
joystick.o: /usr/include/bits/stdlib-ldbl.h /usr/include/stdio.h
joystick.o: /usr/include/libio.h /usr/include/_G_config.h
joystick.o: /usr/include/wchar.h /usr/include/bits/wchar.h
joystick.o: /usr/include/gconv.h /usr/lib/gcc/spu/4.0.2/include/stdarg.h
joystick.o: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
joystick.o: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
joystick.o: /usr/include/ctype.h /usr/include/string.h /usr/include/math.h
joystick.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
joystick.o: /usr/include/bits/mathcalls.h /usr/include/unistd.h
joystick.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
joystick.o: /usr/include/getopt.h /usr/include/fcntl.h
joystick.o: /usr/include/bits/fcntl.h /usr/include/stdint.h
joystick.o: /usr/include/sys/ioctl.h /usr/include/bits/ioctls.h
joystick.o: /usr/include/asm/ioctls.h /usr/include/asm/ioctl.h
joystick.o: /usr/include/bits/ioctl-types.h /usr/include/termios.h
joystick.o: /usr/include/bits/termios.h /usr/include/sys/ttydefaults.h
joystick.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
joystick.o: /usr/include/linux/kd.h /usr/include/linux/types.h
joystick.o: /usr/include/linux/posix_types.h /usr/include/linux/stddef.h
joystick.o: /usr/include/asm/posix_types.h /usr/include/asm/types.h
joystick.o: /usr/include/linux/joystick.h /usr/include/linux/input.h
joystick.o: /usr/include/sys/time.h
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
test.o: ./GL/glspu.h joystick.h

spugl-client/client.o: /usr/include/stdio.h /usr/include/features.h
spugl-client/client.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
spugl-client/client.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
spugl-client/client.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h
spugl-client/client.o: /usr/include/bits/types.h
spugl-client/client.o: /usr/include/bits/typesizes.h /usr/include/libio.h
spugl-client/client.o: /usr/include/_G_config.h /usr/include/wchar.h
spugl-client/client.o: /usr/include/bits/wchar.h /usr/include/gconv.h
spugl-client/client.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
spugl-client/client.o: /usr/include/bits/libio-ldbl.h
spugl-client/client.o: /usr/include/bits/stdio_lim.h
spugl-client/client.o: /usr/include/bits/sys_errlist.h
spugl-client/client.o: /usr/include/bits/stdio-ldbl.h /usr/include/stdlib.h
spugl-client/client.o: /usr/include/sys/types.h /usr/include/time.h
spugl-client/client.o: /usr/include/endian.h /usr/include/bits/endian.h
spugl-client/client.o: /usr/include/sys/select.h /usr/include/bits/select.h
spugl-client/client.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
spugl-client/client.o: /usr/include/sys/sysmacros.h
spugl-client/client.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
spugl-client/client.o: /usr/include/bits/stdlib-ldbl.h /usr/include/unistd.h
spugl-client/client.o: /usr/include/bits/posix_opt.h
spugl-client/client.o: /usr/include/bits/confname.h /usr/include/getopt.h
spugl-client/client.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
spugl-client/client.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
spugl-client/client.o: /usr/lib/gcc/spu/4.0.2/include/limits.h
spugl-client/client.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
spugl-client/client.o: /usr/include/asm/sockios.h /usr/include/sys/un.h
spugl-client/client.o: /usr/include/string.h /usr/include/sys/mman.h
spugl-client/client.o: /usr/include/bits/mman.h /usr/include/sys/stat.h
spugl-client/client.o: /usr/include/bits/stat.h /usr/include/fcntl.h
spugl-client/client.o: /usr/include/bits/fcntl.h spugl-client/daemon.h
spugl-client/client.o: spugl-client/client.h
spugl-server/connection.o: /usr/include/syslog.h /usr/include/sys/syslog.h
spugl-server/connection.o: /usr/include/features.h /usr/include/sys/cdefs.h
spugl-server/connection.o: /usr/include/bits/wordsize.h
spugl-server/connection.o: /usr/include/gnu/stubs.h
spugl-server/connection.o: /usr/include/gnu/stubs-32.h
spugl-server/connection.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
spugl-server/connection.o: /usr/include/bits/syslog-path.h
spugl-server/connection.o: /usr/include/bits/syslog-ldbl.h
spugl-server/connection.o: /usr/include/stdio.h
spugl-server/connection.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h
spugl-server/connection.o: /usr/include/bits/types.h
spugl-server/connection.o: /usr/include/bits/typesizes.h /usr/include/libio.h
spugl-server/connection.o: /usr/include/_G_config.h /usr/include/wchar.h
spugl-server/connection.o: /usr/include/bits/wchar.h /usr/include/gconv.h
spugl-server/connection.o: /usr/include/bits/libio-ldbl.h
spugl-server/connection.o: /usr/include/bits/stdio_lim.h
spugl-server/connection.o: /usr/include/bits/sys_errlist.h
spugl-server/connection.o: /usr/include/bits/stdio-ldbl.h
spugl-server/connection.o: /usr/include/errno.h /usr/include/bits/errno.h
spugl-server/connection.o: /usr/include/linux/errno.h
spugl-server/connection.o: /usr/include/asm/errno.h
spugl-server/connection.o: /usr/include/asm-generic/errno.h
spugl-server/connection.o: /usr/include/asm-generic/errno-base.h
spugl-server/connection.o: /usr/include/unistd.h
spugl-server/connection.o: /usr/include/bits/posix_opt.h
spugl-server/connection.o: /usr/include/bits/confname.h /usr/include/getopt.h
spugl-server/connection.o: /usr/include/stdlib.h /usr/include/sys/types.h
spugl-server/connection.o: /usr/include/time.h /usr/include/endian.h
spugl-server/connection.o: /usr/include/bits/endian.h
spugl-server/connection.o: /usr/include/sys/select.h
spugl-server/connection.o: /usr/include/bits/select.h
spugl-server/connection.o: /usr/include/bits/sigset.h
spugl-server/connection.o: /usr/include/bits/time.h
spugl-server/connection.o: /usr/include/sys/sysmacros.h
spugl-server/connection.o: /usr/include/bits/pthreadtypes.h
spugl-server/connection.o: /usr/include/alloca.h
spugl-server/connection.o: /usr/include/bits/stdlib-ldbl.h
spugl-server/connection.o: /usr/include/string.h /usr/include/fcntl.h
spugl-server/connection.o: /usr/include/bits/fcntl.h /usr/include/sys/mman.h
spugl-server/connection.o: /usr/include/bits/mman.h /usr/include/sys/socket.h
spugl-server/connection.o: /usr/include/sys/uio.h /usr/include/bits/uio.h
spugl-server/connection.o: /usr/include/bits/socket.h
spugl-server/connection.o: /usr/lib/gcc/spu/4.0.2/include/limits.h
spugl-server/connection.o: /usr/include/bits/sockaddr.h
spugl-server/connection.o: /usr/include/asm/socket.h
spugl-server/connection.o: /usr/include/asm/sockios.h
spugl-server/connection.o: spugl-server/connection.h spugl-client/daemon.h
spugl-server/main.o: /usr/include/stdio.h /usr/include/features.h
spugl-server/main.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
spugl-server/main.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
spugl-server/main.o: /usr/lib/gcc/spu/4.0.2/include/stddef.h
spugl-server/main.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
spugl-server/main.o: /usr/include/libio.h /usr/include/_G_config.h
spugl-server/main.o: /usr/include/wchar.h /usr/include/bits/wchar.h
spugl-server/main.o: /usr/include/gconv.h
spugl-server/main.o: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
spugl-server/main.o: /usr/include/bits/libio-ldbl.h
spugl-server/main.o: /usr/include/bits/stdio_lim.h
spugl-server/main.o: /usr/include/bits/sys_errlist.h
spugl-server/main.o: /usr/include/bits/stdio-ldbl.h /usr/include/stdlib.h
spugl-server/main.o: /usr/include/sys/types.h /usr/include/time.h
spugl-server/main.o: /usr/include/endian.h /usr/include/bits/endian.h
spugl-server/main.o: /usr/include/sys/select.h /usr/include/bits/select.h
spugl-server/main.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
spugl-server/main.o: /usr/include/sys/sysmacros.h
spugl-server/main.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
spugl-server/main.o: /usr/include/bits/stdlib-ldbl.h /usr/include/unistd.h
spugl-server/main.o: /usr/include/bits/posix_opt.h
spugl-server/main.o: /usr/include/bits/confname.h /usr/include/getopt.h
spugl-server/main.o: /usr/include/syslog.h /usr/include/sys/syslog.h
spugl-server/main.o: /usr/include/bits/syslog-path.h
spugl-server/main.o: /usr/include/bits/syslog-ldbl.h /usr/include/signal.h
spugl-server/main.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
spugl-server/main.o: /usr/include/bits/sigaction.h
spugl-server/main.o: /usr/include/bits/sigcontext.h
spugl-server/main.o: /usr/include/asm/sigcontext.h /usr/include/asm/ptrace.h
spugl-server/main.o: /usr/include/bits/sigstack.h
spugl-server/main.o: /usr/include/bits/sigthread.h /usr/include/poll.h
spugl-server/main.o: /usr/include/sys/poll.h /usr/include/bits/poll.h
spugl-server/main.o: /usr/include/sys/socket.h /usr/include/sys/uio.h
spugl-server/main.o: /usr/include/bits/uio.h /usr/include/bits/socket.h
spugl-server/main.o: /usr/lib/gcc/spu/4.0.2/include/limits.h
spugl-server/main.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
spugl-server/main.o: /usr/include/asm/sockios.h /usr/include/sys/un.h
spugl-server/main.o: /usr/include/string.h /usr/include/sys/mman.h
spugl-server/main.o: /usr/include/bits/mman.h /usr/include/sys/wait.h
spugl-server/main.o: /usr/include/sys/resource.h /usr/include/bits/resource.h
spugl-server/main.o: /usr/include/bits/waitflags.h
spugl-server/main.o: /usr/include/bits/waitstatus.h spugl-server/connection.h

spufifo.0: spuregs.h struct.h types.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h fifo.h
spufifo.0: gen_spu_command_defs.h queue.h /usr/include/stdio.h
spufifo.0: /usr/include/features.h /usr/include/sys/cdefs.h
spufifo.0: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
spufifo.0: /usr/include/gnu/stubs-32.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/bits/types.h
spufifo.0: /usr/include/bits/typesizes.h /usr/include/libio.h
spufifo.0: /usr/include/_G_config.h /usr/include/wchar.h
spufifo.0: /usr/include/bits/wchar.h /usr/include/gconv.h
spufifo.0: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
spufifo.0: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
spufifo.0: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
spufifo.0: gen_spu_command_exts.h gen_spu_command_table.h
decode.0: /usr/include/stdio.h /usr/include/features.h
decode.0: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
decode.0: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/bits/types.h
decode.0: /usr/include/bits/typesizes.h /usr/include/libio.h
decode.0: /usr/include/_G_config.h /usr/include/wchar.h
decode.0: /usr/include/bits/wchar.h /usr/include/gconv.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
decode.0: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
decode.0: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
decode.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
decode.0: gen_spu_command_defs.h struct.h spuregs.h primitives.h queue.h
decode.0: ./GL/gl.h ./GL/glext.h /usr/include/inttypes.h
decode.0: /usr/include/stdint.h
primitives.0: /usr/include/stdio.h /usr/include/features.h
primitives.0: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
primitives.0: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
primitives.0: /usr/lib/gcc/spu/4.0.2/include/stddef.h
primitives.0: /usr/include/bits/types.h /usr/include/bits/typesizes.h
primitives.0: /usr/include/libio.h /usr/include/_G_config.h
primitives.0: /usr/include/wchar.h /usr/include/bits/wchar.h
primitives.0: /usr/include/gconv.h /usr/lib/gcc/spu/4.0.2/include/stdarg.h
primitives.0: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
primitives.0: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
primitives.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
primitives.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
primitives.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
primitives.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
fragment.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
fragment.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
fragment.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
fragment.0: gen_spu_command_defs.h struct.h spuregs.h primitives.h ./GL/gl.h
fragment.0: ./GL/glext.h /usr/lib/gcc/spu/4.0.2/include/stddef.h
fragment.0: /usr/include/inttypes.h /usr/include/features.h
fragment.0: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
fragment.0: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
fragment.0: /usr/include/stdint.h /usr/include/bits/wchar.h
queue.0: spuregs.h struct.h types.h
queue.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
queue.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h queue.h
queue.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h /usr/include/stdio.h
queue.0: /usr/include/features.h /usr/include/sys/cdefs.h
queue.0: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
queue.0: /usr/include/gnu/stubs-32.h /usr/lib/gcc/spu/4.0.2/include/stddef.h
queue.0: /usr/include/bits/types.h /usr/include/bits/typesizes.h
queue.0: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
queue.0: /usr/include/bits/wchar.h /usr/include/gconv.h
queue.0: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
queue.0: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
queue.0: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
activeblock.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
activeblock.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
activeblock.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
activeblock.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
shader.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
shader.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
shader.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
shader.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
texture.0: /usr/include/stdio.h /usr/include/features.h
texture.0: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
texture.0: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
texture.0: /usr/lib/gcc/spu/4.0.2/include/stddef.h /usr/include/bits/types.h
texture.0: /usr/include/bits/typesizes.h /usr/include/libio.h
texture.0: /usr/include/_G_config.h /usr/include/wchar.h
texture.0: /usr/include/bits/wchar.h /usr/include/gconv.h
texture.0: /usr/lib/gcc/spu/4.0.2/include/stdarg.h
texture.0: /usr/include/bits/libio-ldbl.h /usr/include/bits/stdio_lim.h
texture.0: /usr/include/bits/sys_errlist.h /usr/include/bits/stdio-ldbl.h
texture.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
texture.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
texture.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
texture.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
scaler.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
scaler.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
scaler.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
scaler.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
oldshader.0: /usr/lib/gcc/spu/4.0.2/include/spu_mfcio.h
oldshader.0: /usr/lib/gcc/spu/4.0.2/include/spu_intrinsics.h
oldshader.0: /usr/lib/gcc/spu/4.0.2/include/spu_internals.h fifo.h types.h
oldshader.0: gen_spu_command_defs.h struct.h spuregs.h queue.h
