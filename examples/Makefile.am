# Process this file with automake to create Makefile.in.

noinst_PROGRAMS = scardcontrol
scardcontrol_SOURCES = scardcontrol.c
scardcontrol_LDADD = -lpcsclite

AM_CPPFLAGS = `pkg-config libpcsclite --cflags`

