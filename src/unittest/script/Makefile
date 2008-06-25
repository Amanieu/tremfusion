ROOTDIR=../../
GAMDIR=$(ROOTDIR)/game
SCRIPTDIR=$(ROOTDIR)/script
QCOMDIR=$(ROOTDIR)/qcommon
CFLAGS=-g -Wall

all: test

test: sc_datatype_test
	./$<

sc_datatype_test: sc_datatype_test.c symbols.c $(GAMDIR)/bg_alloc.c $(QCOMDIR)/q_shared.c $(SCRIPTDIR)/sc_datatype.c
	$(CC) $(CFLAGS) -o $@ $+ -I$(SCRIPTDIR)

clean: 
	-rm sc_datatype_test *.o
	
