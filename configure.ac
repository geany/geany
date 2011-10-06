dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_PREREQ([2.60])
AC_INIT([Geany], [1.22],
		[https://sourceforge.net/tracker/?group_id=153444&atid=787791])
AC_CONFIG_SRCDIR([src/geany.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# why do we use this?
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC

AC_PROG_CXX
# check for C++ compiler explicitly and fail if none is found, do this check
# after AC_PROG_CXX has set the CXX environment variable
if ! which $CXX >/dev/null 2>&1; then
	AC_MSG_ERROR([No C++ compiler found. Please install a C++ compiler.])
fi

AC_PROG_INSTALL
AC_PROG_LN_S

# autoscan start

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h fnmatch.h glob.h regex.h stdlib.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.
AC_CHECK_FUNCS([gethostname ftruncate fgetpos mkstemp strerror strstr])

# autoscan end


# check for SVN revision
GEANY_CHECK_REVISION([dnl force debug mode for a SVN working copy
					  CFLAGS="-g -DGEANY_DEBUG $CFLAGS"])


# Check for binary relocation support
GEANY_CHECK_BINRELOC


# GTK/GLib/GIO checks
gtk_modules="gtk+-2.0 >= 2.12 glib-2.0 >= 2.16 gio-2.0 >= 2.16"
PKG_CHECK_MODULES([GTK], [$gtk_modules])
AC_SUBST([GTK_CFLAGS])
AC_SUBST([GTK_LIBS])
GTK_VERSION=`$PKG_CONFIG --modversion gtk+-2.0`
# GTHREAD checks
gthread_modules="gthread-2.0"
PKG_CHECK_MODULES([GTHREAD], [$gthread_modules])
AC_SUBST([GTHREAD_CFLAGS])
AC_SUBST([GTHREAD_LIBS])

# --disable-deprecated switch for GTK2 purification
AC_ARG_ENABLE([deprecated],
		[AS_HELP_STRING([--disable-deprecated], [Disable deprecated GTK functions.])],
		[GTK_CFLAGS="$GTK_CFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"],
		[])


# Plugins support
GEANY_CHECK_PLUGINS


# check whether to use included GNU regex library
GEANY_CHECK_GNU_REGEX


# check for mingw specific settings
GEANY_CHECK_MINGW


GEANY_CHECK_SOCKET


GEANY_CHECK_VTE


# just for a laugh (it has absolutely no effect)
AC_ARG_ENABLE([the-force],
		[AS_HELP_STRING([--enable-the-force],
				[enable if you are Luke Skywalker and the force is with you [default=no]])],
		[be_luke="$enableval"],
		[be_luke="no"])
AC_MSG_CHECKING([whether the force is with you])
if test "x$be_luke" = "xyes"; then
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi


# i18n
GEANY_I18N


# Set ${datadir}
if test "x${datadir}" = 'x${prefix}/share' -o "x${datarootdir}" = 'x${prefix}/share'; then
	if test "x${prefix}" = "xNONE"; then
		prefix=${ac_default_prefix}
	fi
fi

# Set ${docdir} if it is empty
if test -z "${docdir}"; then
	docdir='${datadir}/doc/${PACKAGE}'
	AC_SUBST([docdir])
fi

GEANY_DATA_DIR=`eval echo ${datarootdir}/geany`
AC_SUBST([GEANY_DATA_DIR])



# Output
AC_CONFIG_FILES([
		Makefile
		icons/Makefile
		icons/16x16/Makefile
		icons/48x48/Makefile
		icons/scalable/Makefile
		tagmanager/Makefile
		tagmanager/include/Makefile
		tagmanager/mio/Makefile
		scintilla/Makefile
		scintilla/include/Makefile
		src/Makefile
		plugins/Makefile
		po/Makefile.in
		doc/Makefile
		doc/geany.1
		geany.spec
		geany.pc
		doc/Doxyfile
])
AC_OUTPUT


# Summary
echo "----------------------------------------"
echo "Install Geany in                   : ${prefix}"
if test -n "${build}" -a -n "${target}"; then
	echo "Building Geany on                  : ${build}"
	echo "Building Geany for                 : ${target}"
fi
echo "Using GTK version                  : ${GTK_VERSION}"
echo "Build with plugin support          : ${geany_enable_plugins}"
echo "Use virtual terminal support       : ${geany_enable_vte}"
echo "Use (UNIX domain) socket support   : ${geany_enable_socket}"
if test "x$enable_gnu_regex" = "xyes" ; then
	echo "GNU regex library                  : built-in"
else
	echo "GNU regex library                  : system"
fi

if test "${REVISION}" != "-1"; then
	echo "Compiling Subversion revision      : ${REVISION}"
fi
echo ""
echo "Configuration is done OK."
echo ""
