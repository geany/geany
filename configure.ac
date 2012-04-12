dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([Geany], [1.22],
		[https://sourceforge.net/tracker/?group_id=153444&atid=787791])
AC_CONFIG_SRCDIR([src/geany.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

GEANY_PREFIX
GEANY_DOCDIR

GEANY_STATUS_ADD([Install Geany in], [${prefix}])
if test -n "${build}" -a -n "${target}"; then
	GEANY_STATUS_ADD([Building Geany on], [${build}])
	GEANY_STATUS_ADD([Building Geany for], [${target}])
fi

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
AC_CHECK_HEADERS([fcntl.h fnmatch.h glob.h stdlib.h sys/time.h])

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


# GTK/GLib/GIO checks
gtk_modules="gtk+-2.0 >= 2.16 glib-2.0 >= 2.20"
gtk_modules_private="gio-2.0 >= 2.20 gmodule-2.0"
PKG_CHECK_MODULES([GTK], [$gtk_modules $gtk_modules_private])
AC_SUBST([DEPENDENCIES], [$gtk_modules])
AC_SUBST([GTK_CFLAGS])
AC_SUBST([GTK_LIBS])
GTK_VERSION=`$PKG_CONFIG --modversion gtk+-2.0`
GEANY_STATUS_ADD([Using GTK version], [${GTK_VERSION}])
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


# Check for binary relocation support
GEANY_CHECK_BINRELOC
# CTags source compatibility (we actually use GRegex instead of POSIX regcomp)
AC_DEFINE([HAVE_REGCOMP], [1], [Should always be 1, required for CTags.])
# Plugins support
GEANY_CHECK_PLUGINS
# check for mingw specific settings
GEANY_CHECK_MINGW

GEANY_CHECK_SOCKET
GEANY_CHECK_VTE
GEANY_CHECK_THE_FORCE dnl hehe

# i18n
GEANY_I18N

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
GEANY_STATUS_OUTPUT
echo ""
echo "Configuration is done OK."
echo ""
