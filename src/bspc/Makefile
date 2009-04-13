#
# Makefile for the BSPC tool for the Gladiator Bot
# Intended for gcc/Linux
#
# TTimo 5/15/2001
# some cleanup .. only used on i386 for GtkRadiant setups AFAIK .. removing the i386 tag
# TODO: the intermediate object files should go into their own directory
#   specially for ../botlib and ../qcommon, the compilation flags on those might not be what you expect

#ARCH=i386
CC=gcc
BASE_CFLAGS=-Dstricmp=strcasecmp -DLINUX -DBSPC -DNO_GZIP -U__SSE__ -U__SSE2__

#use these cflags to optimize it
CFLAGS=$(BASE_CFLAGS) -O6 -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations -falign-loops=2 \
	-falign-jumps=2 -falign-functions=2
#use these when debugging 
#CFLAGS=$(BASE_CFLAGS) -ggdb3 -O0

LDFLAGS=-ldl -lm -lpthread

B=build

DO_CC=$(CC) $(CFLAGS) -o $@ -c $<

#############################################################################
# SETUP AND BUILD BSPC
#############################################################################

$(B)/%.o: %.c
	$(DO_CC)

$(B)/%.o: ../botlib/%.c
	$(DO_CC)

$(B)/%.o: ../qcommon/%.c
	$(DO_CC)

$(B)/%.o: ../zlib/%.c
	$(DO_CC)

GAME_OBJS = \
	$(B)/_files.o\
	$(B)/aas_areamerging.o\
	$(B)/aas_cfg.o\
	$(B)/aas_create.o\
	$(B)/aas_edgemelting.o\
	$(B)/aas_facemerging.o\
	$(B)/aas_file.o\
	$(B)/aas_gsubdiv.o\
	$(B)/aas_map.o\
	$(B)/aas_prunenodes.o\
	$(B)/aas_store.o\
	$(B)/be_aas_bspc.o\
	$(B)/be_aas_bspq3.o\
	$(B)/be_aas_cluster.o\
	$(B)/be_aas_move.o\
	$(B)/be_aas_optimize.o\
	$(B)/be_aas_reach.o\
	$(B)/be_aas_sample.o\
	$(B)/brushbsp.o\
	$(B)/bspc.o\
	$(B)/cm_load.o\
	$(B)/cm_patch.o\
	$(B)/cm_test.o\
	$(B)/cm_trace.o\
	$(B)/csg.o\
	$(B)/glfile.o\
	$(B)/l_bsp_ent.o\
	$(B)/l_bsp_hl.o\
	$(B)/l_bsp_q1.o\
	$(B)/l_bsp_q2.o\
	$(B)/l_bsp_q3.o\
	$(B)/l_bsp_sin.o\
	$(B)/l_cmd.o\
	$(B)/l_libvar.o\
	$(B)/l_log.o\
	$(B)/l_math.o\
	$(B)/l_mem.o\
	$(B)/l_poly.o\
	$(B)/l_precomp.o\
	$(B)/l_qfiles.o\
	$(B)/l_script.o\
	$(B)/l_struct.o\
	$(B)/l_threads.o\
	$(B)/l_utils.o\
	$(B)/leakfile.o\
	$(B)/map.o\
	$(B)/map_hl.o\
	$(B)/map_q1.o\
	$(B)/map_q2.o\
	$(B)/map_q3.o\
	$(B)/map_sin.o\
	$(B)/md4.o\
	$(B)/nodraw.o\
	$(B)/portals.o\
	$(B)/textures.o\
	$(B)/tree.o\
	$(B)/unzip.o\
	$(B)/ioapi.o\
	$(B)/adler32.o\
	$(B)/inffast.o\
	$(B)/inflate.o\
	$(B)/inftrees.o\
	$(B)/zutil.o

        #tetrahedron.o

bspc: makedirs $(GAME_OBJS)
	$(CC) $(CFLAGS) -o $@ $(GAME_OBJS) $(LDFLAGS)
	strip $@

makedirs:
	@mkdir -p $(B)


#############################################################################
# MISC
#############################################################################

clean:
	-rm -f $(GAME_OBJS)

depend:
	gcc -MM $(GAME_OBJS:.o=.c)

#install:
#	cp bspci386 ..

#
# From "make depend"
#

