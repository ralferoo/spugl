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

TARGETS = spugld testclient

LIBDIRS = -L/usr/lib

#LIBS = -lm -lspe -lpthread -lrt
#LIBSPE2 = 

LIBS = -lm -lspe2 -lpthread -lrt
LIBSPE2 = -DUSE_LIBSPE2

USERLAND = 32
#USERLAND = 64

PPUPREFIX = ppu-

PPUAR = $(PPUPREFIX)ar
PPUEMBEDSPU = $(PPUPREFIX)embedspu
PPUSTRIP = $(PPUPREFIX)strip

PPUCC = $(PPUPREFIX)gcc
PPUCCFLAGS = -c -ggdb $(LIBSPE2) -I. -Wno-trigraphs -std=gnu99
PPUCCFLAGSARCH =-m$(USERLAND) -DUSERLAND_$(USERLAND)_BITS

NEWSPUCC = cellgcc -DUSERLAND_$(USERLAND)_BITS -std=gnu99 -I/usr/include
SPUCC = spu-gcc
SPUCCFLAGSARCH = -DUSERLAND_$(USERLAND)_BITS -std=gnu99 -fpic -I.
SPUCCFLAGS = -O6 -DSPU_REGS

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

SPU_DRIVER_SOURCES := $(wildcard server/spu/*.c)
SPU_DRIVER_TARGETS := $(patsubst %.c,%.0,$(SPU_DRIVER_SOURCES))
SPU_DRIVER_HNDL = server/spu_main.handle.o$(USERLAND)
SPU_DRIVER_HNDL_BASE = $(patsubst %.o$(USERLAND),%.spe,$(SPU_DRIVER_HNDL))

RENDER_DRIVER_SOURCES := $(wildcard server/renderspu/*.c)
RENDER_DRIVER_TARGETS := $(patsubst %.c,%.0,$(RENDER_DRIVER_SOURCES))
RENDER_DRIVER_HNDL = server/render_main.handle.o$(USERLAND)
RENDER_DRIVER_HNDL_BASE = $(patsubst %.o$(USERLAND),%.spe,$(RENDER_DRIVER_HNDL))

DAEMON_TARGETS_C := $(wildcard server/*.c)
DAEMON_TARGETS_H := $(wildcard server/*.h)
DAEMON_TARGETS := $(patsubst %.c,%.o,$(DAEMON_TARGETS_C))

CLIENT_TARGETS32 = testclient.o32 spugl.a32
CLIENT_TARGETS64 = testclient.o64 spugl.a64

CLIENT_LIB_TARGETS_C := $(wildcard client/*.c)
CLIENT_LIB_TARGETS_H := $(wildcard client/*.h)
CLIENT_LIB_TARGETS32 := $(patsubst %.c,%.o32,$(CLIENT_LIB_TARGETS_C))
CLIENT_LIB_TARGETS64 := $(patsubst %.c,%.o64,$(CLIENT_LIB_TARGETS_C))

all:	$(TARGETS)

.FORCE:

server:	spugld .FORCE
	./spugld

client:	testclient .FORCE
	./testclient

client64:	testclient64 .FORCE
	./testclient64

spugld:	spugld.debug
	cp $< $@
	$(PPUSTRIP) $@

spugld.debug:	$(DAEMON_TARGETS) $(SPU_DRIVER_HNDL) $(RENDER_DRIVER_HNDL)
	$(PPUCC) -m$(USERLAND) -o $@ $(DAEMON_TARGETS) $(SPU_DRIVER_HNDL) $(RENDER_DRIVER_HNDL) $(LIBS)

testclient:	$(CLIENT_TARGETS32)
	$(PPUCC) -m32 -o $@ $(CLIENT_TARGETS32)

testclient64:	$(CLIENT_TARGETS64)
	$(PPUCC) -m64 -o $@ $(CLIENT_TARGETS64)

spugl.a32:	$(CLIENT_LIB_TARGETS32)
	$(PPUAR) -r $@ $(CLIENT_LIB_TARGETS32)

spugl.a64:	$(CLIENT_LIB_TARGETS64)
	$(PPUAR) -r $@ $(CLIENT_LIB_TARGETS64)

test:	$(PPU_TEST_OBJS) $(SPU_HNDL)
	$(PPUCC) -m$(USERLAND) -o test $(PPU_TEST_OBJS) $(SPU_HNDL) $(LIBDIRS) $(LIBS)

texmap.sqf:	test
	strip $<
	rm -rf /tmp/texmap-build
	mkdir /tmp/texmap-build
	cp $< /tmp/texmap-build/launch
	strip /tmp/texmap-build/launch
	echo 'Texture Mapping Demo':`date '+%s'` >/tmp/texmap-build/.version
	mksquashfs /tmp/texmap-build $@ -noappend
	
client/spuglver.h: .git
	./version.sh

test.static:	$(PPU_TEST_OBJS) $(SPU_HNDL)
	$(PPUCC) -m$(USERLAND) -o test.static $(PPU_TEST_OBJS) $(SPU_HNDL) $(LIBS) -static

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
	gvim -p Makefile testclient.c client/glfifo.c server/spu/*.[ch] server/renderspu/*.[ch] &
#	gvim -p shader.c texture.c queue.h test.c struct.h glfifo.c textureprep.c decode.c primitives.c

source:
	make shader.s && less shader.s

GENPRODUCTS = gen_spu_command_defs.h gen_spu_command_exts.h gen_spu_command_table.h

ppufifo.o: Makefile

shader.s: queue.h
#ppufifo.o: Makefile .gen
spufifo.0: Makefile $(GENPRODUCTS)

NEW_GEN_SOURCES := $(wildcard server/spu/*_cmd.c)
client/gen_command_defs.h: .gennew
server/spu/gen_command_exts.inc: .gennew
server/spu/gen_command_table.inc: .gennew
.gennew: server/importdefs.pl $(NEW_GEN_SOURCES)
	perl server/importdefs.pl $(NEW_GEN_SOURCES)
	@touch .gennew

gen_spu_command_defs.h: .gen
gen_spu_command_exts.h: .gen
gen_spu_command_table.h: .gen
.gen: $(GENSOURCES) importdefs.pl
	perl importdefs.pl $(GENSOURCES)
	@touch .gen

spu_3d.handle.spe: $(SPU_OBJS) Makefile
	$(SPUCC) $(SPUCCFLAGSARCH) $(SPU_OBJS) -o spu_3d.handle.spe
	spu-strip spu_3d.handle.spe

server/spu_main.handle.spe: $(SPU_DRIVER_TARGETS) Makefile
	$(SPUCC) $(SPUCCFLAGSARCH) $(SPU_DRIVER_TARGETS) -o server/spu_main.handle.spe
	spu-strip server/spu_main.handle.spe

server/render_main.handle.spe: $(RENDER_DRIVER_TARGETS) Makefile
	$(SPUCC) $(SPUCCFLAGSARCH) $(RENDER_DRIVER_TARGETS) -o server/render_main.handle.spe
	spu-strip server/render_main.handle.spe

###############################################################################
#
# New style dependency checking

.depend_ppu: $(PPU_SRCS)
	@makedepend -f- -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(PPU_SRCS) -DUSERLAND_$(USERLAND)_BITS >$@

.depend_o32: $(CLIENT_LIB_TARGETS_C)
	@makedepend -f- -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(CLIENT_LIB_TARGETS_C) -o.o32 -DUSERLAND_$(USERLAND)_BITS >$@

.depend_o64: $(CLIENT_LIB_TARGETS_C)
	@makedepend -f- -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(CLIENT_LIB_TARGETS_C) -o.o64 -DUSERLAND_$(USERLAND)_BITS >$@

.depend_daemon: $(DAEMON_TARGETS_C)
	@makedepend -f- -I/usr/lib/gcc/spu/4.0.2/include/  -I. $(DAEMON_TARGETS_C) >$@

.depend_spu: $(SPU_SRCS)
	@makedepend -f- -I/usr/lib/gcc/spu/4.0.2/include/ -I. -o.0 $(SPU_SRCS) -DSPU_REGS -DUSERLAND_$(USERLAND)_BITS >$@
	@for i in $(SPU_OBJS) ; do grep $$i:.*spuregs.h $@ >/dev/null || [ ! -f `basename $$i .0`.c ] || ( echo ERROR: $$i does not refer to spuregs.h && false) ; done

.depend_spudriver: $(SPU_SRCS)
	@makedepend -f- -I/usr/lib/gcc/spu/4.0.2/include/ -I. -o.0 $(SPU_DRIVER_SOURCES) -DSPU_REGS -DUSERLAND_$(USERLAND)_BITS >$@

.depend_renderdriver: $(RENDER_DRIVER_SRCS)
	@makedepend -f- -I/usr/lib/gcc/spu/4.0.2/include/ -I. -o.0 $(RENDER_DRIVER_SOURCES) -DSPU_REGS -DUSERLAND_$(USERLAND)_BITS >$@

###############################################################################
#
# include the generated dependencies

-include .depend_ppu .depend_o32 .depend_o64 .depend_daemon .depend_spu .depend_spudriver .depend_renderdriver

###############################################################################
#
# SPU rules

%.s: %.c
	$(SPUCC) $(SPUCCFLAGSARCH) $(SPUCCFLAGS) -c -S $< -o $*.s

%.0: %.c
	$(SPUCC) $(SPUCCFLAGSARCH) $(SPUCCFLAGS) -c $< -o $*.0

%.0: %.s
	$(SPUCC) $(SPUCCFLAGSARCH) $(SPUCCFLAGS) -c $< -o $*.0

%.handle.o$(USERLAND): %.handle.spe
	$(PPUEMBEDSPU) `basename $*_handle` $*.handle.spe $*.handle.o$(USERLAND)

### BUILD-ONLY-END ###

###############################################################################
#
# useful targets

clean:
	rm -f *.o
#	rm -f *.spe *.o$(USERLAND)
	rm -f *.0 *.o32 *.o64
	rm -rf build dist
	rm -f .gen .gennew test
	rm -f textures/*.o
	rm -f spugld spugld.debug testclient testclient64 spugl-*/*.o spugl.a spugl.a32 spugl.a64
	rm -f hilbert
	rm -f server/*.o client/*.o client/*.o32 client/*.o64
	rm -f server/spu/*.0 client/spu/*.0
	rm -f server/renderspu/*.0 client/renderspu/*.0
	rm -f .depend_*

# gen_spu_command_defs.h gen_spu_command_exts.h gen_spu_command_table.h

###############################################################################
#
# rules

%.o: %.c
	$(PPUCC) $(PPUCCFLAGS) $(PPUCCFLAGSARCH) $< -o $@

%.o: %.cpp
	$(PPUCC) $(PPUCCFLAGS) $(PPUCCFLAGSARCH) $< -o $@

%.o32: %.c
	$(PPUCC) $(PPUCCFLAGS) -m32 $< -o $@

%.o32: %.cpp
	$(PPUCC) $(PPUCCFLAGS) -m32 $< -o $@

%.o64: %.c
	$(PPUCC) $(PPUCCFLAGS) -m64 $< -o $@

%.o64: %.cpp
	$(PPUCC) $(PPUCCFLAGS) -m64 $< -o $@

###############################################################################
#
# use "make depend" to update this
#
# DO NOT DELETE
