ROOTDIR=../../
GAMDIR=$(ROOTDIR)/game
SCRIPTDIR=$(ROOTDIR)/script
QCOMDIR=$(ROOTDIR)/qcommon
PYTHONDIR=$(ROOTDIR)/python
CFLAGS=-g -Wall

ifndef TEST_PYTHON
	TEST_PYTHON=0
endif

all: test

test: sc_datatype_test
	./$<

OBJ= sc_datatype_test.c symbols.c $(GAMDIR)/bg_alloc.c $(QCOMDIR)/q_shared.c $(SCRIPTDIR)/sc_datatype.c
LDFLAGS=
ifeq ($(TEST_PYTHON),1)
  CFLAGS += -DTEST_PYTHON
  OBJ+= sc_python_test.c $(SCRIPTDIR)/sc_python.c
endif



sc_datatype_test: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $+ $(LDFLAGS) -I$(SCRIPTDIR)

clean: 
	@echo "CLEAN $(B)"
	@rm -f $(OBJ)
	
