# Process this file with automake to create Makefile.in.

noinst_PROGRAMS = scardcontrol
scardcontrol_SOURCES = scardcontrol.c
scardcontrol_CFLAGS = $(PCSC_CFLAGS) $(PTHREAD_CFLAGS)
scardcontrol_LDADD = $(PCSC_LIBS) $(PTHREAD_LIBS)

