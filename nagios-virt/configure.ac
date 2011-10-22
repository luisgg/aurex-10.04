dnl Process this file with autoconf to produce a configure script.

AC_INIT(nagios-virt,0.2.0)

dnl Check for basic C environment.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_CPP

AC_C_PROTOTYPES
test "x$U" != "x" && AC_MSG_ERROR(Compiler not ANSI compliant)

AC_PROG_CC_C_O

dnl If gcc, use -Wall -Werror
if test "x$CC" = "xgcc"; then
  CFLAGS="$CFLAGS -Wall -Werror"
fi

dnl Check for some required functions.
AC_CHECK_FUNCS([getopt_long basename],,
	AC_MSG_ERROR([Missing function $ac_func]))

dnl Check for libxml2 (taken from libvirt, originally from xmlsec)
m4_pattern_allow([PKG_CONFIG_ENABLED])
LIBXML_MIN_VERSION="2.5.0"
LIBXML_CONFIG="xml2-config"
LIBXML_CFLAGS=""
LIBXML_LIBS=""
LIBXML_FOUND="no"
AC_ARG_WITH(libxml,
	AC_HELP_STRING([--with-libxml=PATH],[Set location of libxml2]))
if test "z$with_libxml" = "zno" ; then 
  AC_MSG_ERROR(libxml2 >= $LIBXML_MIN_VERSION is required)
elif test "z$with_libxml" = "z" -a "z$PKG_CONFIG_ENABLED" = "zyes" ; then
  PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= $LIBXML_MIN_VERSION,
	[LIBXML_FOUND=yes],
	[LIBXML_FOUND=no])
fi
if test "z$LIBXML_FOUND" = "zno" ; then
  AC_MSG_CHECKING(for libxml2 libraries >= $LIBXML_MIN_VERSION) 
  if test "z$with_libxml" != "z" ; then 
    LIBXML_CONFIG=$with_libxml/bin/$LIBXML_CONFIG
  fi
  if ! $LIBXML_CONFIG --version > /dev/null 2>&1 ; then
    AC_MSG_ERROR(Could not find libxml2 anywhere (see config.log for details).)
  fi
  vers=`$LIBXML_CONFIG --version | awk -F. '{ printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
  minvers=`echo $LIBXML_MIN_VERSION | awk -F. '{ printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
  if test "$vers" -ge "$minvers" ; then
      LIBXML_LIBS="`$LIBXML_CONFIG --libs`"
      LIBXML_CFLAGS="`$LIBXML_CONFIG --cflags`"
      LIBXML_FOUND="yes"
      AC_MSG_RESULT(yes)
  else
      AC_MSG_ERROR(You need at least libxml2 >= $LIBXML_MIN_VERSION)
  fi
fi

CFLAGS="$CFLAGS $LIBXML_CFLAGS"
LIBS="$LIBS $LIBXML_LIBS"

dnl Check for libvirt development environment.
AC_ARG_WITH(libvirt,
	AC_HELP_STRING([--with-libvirt=PATH],[Set path to installed libvirt]),
	[if test "x$withval" != "x"; then
	   CFLAGS="$CFLAGS -I$withval/include"
	   LDFLAGS="$LDFLAGS -L$withval/lib"
	 fi
	])
AC_CHECK_LIB(virt,virConnectOpen,
	[],
	AC_MSG_ERROR([You must install libvirt library]))
AC_CHECK_HEADER([libvirt/libvirt.h],
	[],
	AC_MSG_ERROR([You must install libvirt development package]))

dnl Check for libvirt >= 0.2.1 (our minimum supported version).
dnl See: http://libvirt.org/hvsupport.html
AC_CHECK_FUNC(virConnectGetCapabilities,
	[],
	AC_MSG_ERROR([You must have libvirt >= 0.2.1]))

dnl Check for location of nagios program.
AC_PATH_PROG(NAGIOS,nagios,[no],[/sbin:/usr/sbin:/usr/local/sbin:$PATH])
if test "x$NAGIOS" = "xno"; then
   	AC_MSG_ERROR([Cannot find `nagios' program])
fi

dnl Where are nagios configuration files installed?
AC_ARG_WITH(nagios-configdir,
	AC_HELP_STRING([--with-nagios-configdir=PATH],[Set path to nagios configuration directory]),
	[if test "x$withval" != "x"; then
	   NAGIOS_CONFIGDIR="$withval"
	 fi
	])
dnl If --with-nagios-configdir wasn't given, then try to find it.
test "x$NAGIOS_CONFIGDIR" = "x" && {
AC_MSG_CHECKING([for nagios config dir])
for d in /etc/nagios /usr/etc/nagios /usr/local/etc/nagios /opt/nagios/etc; do
  if test -d $d; then
    NAGIOS_CONFIGDIR="$d"
    AC_MSG_RESULT([$d])
    break
  fi
done
}
test "x$NAGIOS_CONFIGDIR" = "x" && \
  AC_MSG_ERROR([Cannot find Nagios configuration directory])

dnl Extra variables in Makefile.
AC_SUBST(NAGIOS_CONFIGDIR)

dnl Extra variables in header file (config.h).
AC_DEFINE_UNQUOTED([NAGIOS_CONFIGDIR],["$NAGIOS_CONFIGDIR"],
	[Nagios configuration directory])
AC_DEFINE_UNQUOTED([NAGIOS],["$NAGIOS"],
	[Nagios binary])

dnl Produce output files.
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
