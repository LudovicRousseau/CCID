# Process this file with autoconf to produce a configure script.
# You may need to use autoconf 2.56 or newer

# Require autoconf 2.61
AC_PREREQ([2.69])

AC_INIT([ccid],[1.4.31])
AC_CONFIG_SRCDIR(src/ifdhandler.c)
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE(1.8 dist-bzip2 no-dist-gzip subdir-objects)
AC_CONFIG_MACRO_DIR([m4])

# silent build by default
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Default install dir
AC_PREFIX_DEFAULT(/usr/local)

# Automake boilerplate.
AC_CANONICAL_HOST

# create a config.h file (Automake will add -DHAVE_CONFIG_H)
AC_CONFIG_HEADERS([config.h])

# Options
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AM_PROG_LEX
AM_PROG_AR
PKG_PROG_PKG_CONFIG

# check pcsc-lite version
PCSC_NEEDED_VERSION="1.8.3"
PKG_CHECK_EXISTS([libpcsclite],
	[PKG_CHECK_MODULES(PCSC, libpcsclite >= $PCSC_NEEDED_VERSION, [],
	[
	if test -f /usr/local/lib/pkgconfig/libpcsclite.pc -a "x$PKG_CONFIG" != x ; then
		AC_MSG_ERROR([use PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ./configure])
	  else
		AC_MSG_WARN([install pcsc-lite $PCSC_NEEDED_VERSION or later])
	  fi
	])],
	[AC_MSG_WARN([libpcsclite not found by pkg-config])]
)

saved_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $PCSC_CFLAGS"
PCSC_ERROR_MSG="install pcsc-lite $PCSC_NEEDED_VERSION or later, or use ./configure PCSC_CFLAGS=..."
AC_CHECK_HEADER(ifdhandler.h,, [AC_MSG_ERROR([$PCSC_ERROR_MSG])])
AC_CHECK_HEADER(reader.h,, [AC_MSG_ERROR([$PCSC_ERROR_MSG])])
CPPFLAGS="$saved_CPPFLAGS"

# Add libtool support.
# Static lib is disabled by default. Use --enable-static if needed
LT_INIT(disable-static)
LT_INIT

# Automatically update the libtool script if it becomes out-of-date.
AC_SUBST(LIBTOOL_DEPS)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h fcntl.h stdlib.h unistd.h termios.h string.h sys/time.h sys/types.h stdarg.h arpa/inet.h stdio.h,,
	[AC_MSG_ERROR([some header files not found])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_CHECK_FUNCS(select strerror strncpy memcpy strlcpy strlcat)

# Select OS specific versions of source files.
AC_SUBST(BUNDLE_HOST)
AC_SUBST(DYN_LIB_EXT)
BUNDLE_HOST=`uname | sed -e s,/,_,`
DYN_LIB_EXT="so"
case "$BUNDLE_HOST" in
Darwin)
	BUNDLE_HOST=MacOS
	DYN_LIB_EXT="dylib"
	;;
SunOS)
	BUNDLE_HOST=Solaris
	;;
	esac

# --disable-libusb
AC_ARG_ENABLE(libusb,
	AS_HELP_STRING([--disable-libusb],[do not use libusb]),
	[ use_libusb="${enableval}" ], [ use_libusb=yes ] )

# check if libusb is used
LIBUSB_NEEDED_VERSION="1.0.9"
if test "x$use_libusb" != xno ; then
	PKG_CHECK_EXISTS([libusb-1.0], [
	PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= $LIBUSB_NEEDED_VERSION, [],
		[
		AC_MSG_WARN([install libusb $LIBUSB_NEEDED_VERSION or later])
		PKG_CHECK_MODULES(LIBUSB, libusb-1.0)
		])
	])

	saved_CPPFLAGS="$CPPFLAGS"
	saved_LIBS="$LIBS"

	CPPFLAGS="$CPPFLAGS $LIBUSB_CFLAGS"
	LIBS="$LDFLAGS $LIBUSB_LIBS"

	AC_CHECK_HEADERS(libusb.h, [],
		[ AC_MSG_ERROR([libusb.h not found, install libusb or use ./configure LIBUSB_CFLAGS=...]) ])

	AC_MSG_CHECKING([for libusb_init])
	AC_TRY_LINK_FUNC(libusb_init, [ AC_MSG_RESULT([yes]) ],
		[ AC_MSG_ERROR([libusb not found, use ./configure LIBUSB_LIBS=...]) ])

	CPPFLAGS="$saved_CPPFLAGS"
	LIBS="$saved_LIBS"

	use_libusb=yes
fi
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)
AM_CONDITIONAL(WITH_LIBUSB, test "${use_libusb}" != "no")

# --enable-composite-as-multislot
use_composite_as_multislot=no
AC_ARG_ENABLE(composite-as-multislot,
	AS_HELP_STRING([--enable-composite-as-multislot],[composite device are seen as multi-slots]),
	[ use_composite_as_multislot="${enableval}" ] )

if test "x$use_composite_as_multislot" = xyes; then
	AC_DEFINE(USE_COMPOSITE_AS_MULTISLOT, 1, [composite device are seen as multi-slots])
fi

# check if the compiler support -fvisibility=hidden (GCC >= 4)
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden"
AC_MSG_CHECKING([for -fvisibility=hidden])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([char foo;])],
    [   AC_MSG_RESULT([yes])
        SYMBOL_VISIBILITY="-fvisibility=hidden" ],
    AC_MSG_RESULT([no]))
CFLAGS="$saved_CFLAGS"
AC_SUBST(SYMBOL_VISIBILITY)

# --disable-multi-thread
AC_ARG_ENABLE(multi-thread,
	AS_HELP_STRING([--disable-multi-thread],[disable multi threading]),
	[ multithread="${enableval}" ], [ multithread=yes ] )

if test "${multithread}" != no ; then
	AX_PTHREAD(
		[ AC_DEFINE(HAVE_PTHREAD, 1,
			[Define if you have POSIX threads libraries and header files.])
	   	], [ AC_MSG_ERROR([POSIX thread support required]) ])

	multithread=yes
fi

# --enable-bundle=NAME
AC_ARG_ENABLE(bundle,
	AS_HELP_STRING([--enable-bundle=NAME],[bundle directory name
	(default ifd-ccid.bundle)]),
	[bundle="${enableval}"], [bundle=false])
if test "${bundle}" = false ; then
	bundle="ifd-ccid.bundle"
fi
AC_DEFINE_UNQUOTED(BUNDLE, "$bundle", [bundle directory name])

# --enable-usbdropdir=DIR
AC_ARG_ENABLE(usbdropdir,
	AS_HELP_STRING([--enable-usbdropdir=DIR],[directory containing USB
	drivers (default to pcscd config or $(prefix)/pcsc/drivers)]),
	[usbdropdir="${enableval}"], [usbdropdir=false])
if test "${usbdropdir}" = false ; then
	usbdropdir=`$PKG_CONFIG libpcsclite --variable=usbdropdir`
fi
AC_DEFINE_UNQUOTED(PCSCLITE_HP_DROPDIR, "$usbdropdir", [directory containing USB drivers])
if test "${usbdropdir}" = "" ; then
	AC_MSG_ERROR([use --enable-usbdropdir=DIR])
fi

# --enable-twinserial
AC_ARG_ENABLE(twinserial,
	AS_HELP_STRING([--enable-twinserial],[also compile and install the serial Twin driver]),
	[twinserial="${enableval}"], [twinserial=no])
AM_CONDITIONAL(WITH_TWIN_SERIAL, test "${twinserial}" != "no")

# --enable-ccidtwindir=DIR
AC_ARG_ENABLE(ccidtwindir,
	AS_HELP_STRING([--enable-ccidtwindir=DIR],[directory to install the
	serial Twin driver (default to pcscd config or $(prefix)/pcsc/drivers/serial)]),
	[ccidtwindir="${enableval}"], [ccidtwindir=false])
if test "${ccidtwindir}" = false ; then
	ccidtwindir=$usbdropdir/serial
fi

# --enable-serialconfdir=DIR
AC_ARG_ENABLE(serialconfdir,
	AS_HELP_STRING([--enable-serialconfdir=dir],[directory containing
		serial drivers (default to pcscd config)]),
	[serialconfdir="${enableval}"], [serialconfdir=false])
if test "${serialconfdir}" = false ; then
	serialconfdir=`$PKG_CONFIG libpcsclite --variable=serialconfdir`
fi

# --disable-pcsclite
AC_ARG_ENABLE(pcsclite,
	AS_HELP_STRING([--disable-pcsclite],[do not use pcsc-lite debug support]),
	[ pcsclite="${enableval}" ], [ pcsclite=yes ] )

if test "${pcsclite}" != no ; then
	# check that pcsc-lite is installed
	OLD_LIBS="$LIBS"
	OLD_CFLAGS="$CFLAGS"
	LIBS="$LIBS $PCSC_LIBS"
	CFLAGS="$CFLAGS $PCSC_CFLAGS"
	AC_MSG_CHECKING([for SCardEstablishContext])
	AC_TRY_LINK_FUNC(SCardEstablishContext,
	    [ AC_MSG_RESULT([yes]) ],
		[ AC_MSG_ERROR([SCardEstablishContext() not found, install pcsc-lite, or use PCSC_LIBS=...  ./configure]) ])
	LIBS="$OLD_LIBS"
	CFLAGS="$OLD_CFLAGS"

	pcsclite=yes
fi
AM_CONDITIONAL(WITHOUT_PCSC, test "${pcsclite}" != "yes")



# --enable-syslog
AC_ARG_ENABLE(syslog,
	AS_HELP_STRING([--enable-syslog],[use syslog instead of printf for debug (Yosemite)]),
	[ use_syslog="${enableval}" ], [ use_syslog=no ] )

if test x$use_syslog = xyes; then
  AC_DEFINE(USE_SYSLOG, 1, [Use syslog(3) for debug])
fi


# --disable-class
AC_ARG_ENABLE(class,
	AS_HELP_STRING([--disable-class],[remove the CCIDCLASSDRIVER from Info.plist]),
	[class="${enableval}"], [class=yes])
if test "${class}" != yes ; then
	NOCLASS="--no-class"
fi
AC_SUBST(NOCLASS)

# --enable-embedded
AC_ARG_ENABLE(embedded,
  AS_HELP_STRING([--enable-embedded],[limit RAM and CPU ressources by disabling features (log)]),
  [ use_embedded="${enableval}" ])

if test x$use_embedded = xyes; then
  AC_DEFINE(NO_LOG, 1, [Disable logging support])
fi

# --enable-zlp
AC_ARG_ENABLE(zlp,
  AS_HELP_STRING([--enable-zlp],[enable the Zero Length Packet patch for some Gemalto readers]),
  [ use_zlp="${enableval}" ])

if test x$use_zlp = xyes; then
  AC_DEFINE(ENABLE_ZLP, 1, [Enable Zero Length Packet patch])
fi

# Setup dist stuff
AC_SUBST(ac_aux_dir)
AC_SUBST(bundle)
AC_SUBST(usbdropdir)
AC_SUBST(ccidtwindir)
AC_SUBST(serialconfdir)
AS_AC_EXPAND(bindir_exp,$bindir)
AS_AC_EXPAND(sysconfdir_exp,$sysconfdir)

cat << EOF

libccid has been configured with following options:

Version:             ${PACKAGE_VERSION}
User binaries:       $(eval eval eval echo "${bindir_exp}")
Configuration files: $(eval eval eval echo "${sysconfdir_exp}")


Host:                ${host}
Compiler:            ${CC}
Preprocessor flags:  ${CPPFLAGS}
Compiler flags:      ${CFLAGS}
Preprocessor flags:  ${CPPFLAGS}
Linker flags:        ${LDFLAGS}
Libraries:           ${LIBS}

PCSC_CFLAGS:         ${PCSC_CFLAGS}
PCSC_LIBS:           ${PCSC_LIBS}
PTHREAD_CFLAGS:      ${PTHREAD_CFLAGS}
PTHREAD_LIBS:        ${PTHREAD_LIBS}
BUNDLE_HOST:         ${BUNDLE_HOST}
DYN_LIB_EXT:         ${DYN_LIB_EXT}
LIBUSB_CFLAGS:       ${LIBUSB_CFLAGS}
LIBUSB_LIBS:         ${LIBUSB_LIBS}
SYMBOL_VISIBILITY:   ${SYMBOL_VISIBILITY}
NOCLASS:             ${NOCLASS}

libusb support:          ${use_libusb}
composite as multislot:  ${use_composite_as_multislot}
multi threading:         ${multithread}
bundle directory name:   ${bundle}
USB drop directory:      ${usbdropdir}
serial Twin support:     ${twinserial}
serial twin install dir: ${ccidtwindir}
serial config directory: ${serialconfdir}
compiled for pcsc-lite:  ${pcsclite}
syslog debug:            ${use_syslog}
class driver:            ${class}

EOF

# Write Makefiles.
AC_CONFIG_FILES(Makefile
	src/Makefile
	readers/Makefile
	contrib/Makefile
	contrib/Kobil_mIDentity_switch/Makefile
	contrib/RSA_SecurID/Makefile
	examples/Makefile)

AC_OUTPUT

