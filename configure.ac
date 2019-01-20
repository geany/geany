dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([Geany], [1.35],
		[https://github.com/geany/geany/issues])
AC_CONFIG_SRCDIR([src/geany.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 -Wall parallel-tests subdir-objects])
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

m4_ifdef([AM_PROG_AR],[AM_PROG_AR])
LT_INIT([disable-static])
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O

AC_PROG_CXX
GEANY_PROG_CXX
AX_CXX_COMPILE_STDCXX_11

AC_PROG_INSTALL
AC_PROG_LN_S

# autoscan start

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h glob.h stdlib.h sys/time.h errno.h limits.h])

# Checks for dependencies needed by ctags
AC_CHECK_HEADERS([fnmatch.h direct.h io.h sys/dir.h])
AC_DEFINE([USE_STDBOOL_H], [1], [whether or not to use <stdbool.h>.])
AC_DEFINE([CTAGS_LIB], [1], [compile ctags as a library.])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.
AC_CHECK_FUNCS([fgetpos fnmatch mkstemp strerror strstr])

# Function checks for u-ctags
AC_CHECK_FUNCS([strcasecmp stricmp], [break])
AC_CHECK_FUNCS([strncasecmp strnicmp], [break])
AC_CHECK_FUNCS([truncate ftruncate chsize], [break])
# non-functions checks for u-ctags.  Not that we really need those as we don't
# use u-ctags's main, but the corresponding macros have to be defined to
# something, so simply perform the actual checks.
AC_CHECK_DECLS([__environ],,,[[#include <unistd.h>]])
AC_CHECK_DECLS([_NSGetEnviron],,,[[#include <crt_externs.h>]])

# autoscan end


# check for VCS revision
GEANY_CHECK_REVISION([dnl force debug mode for a VCS working copy
					  CFLAGS="-g -DGEANY_DEBUG $CFLAGS"])

# GTK version check
AC_ARG_ENABLE([gtk3],
		[AS_HELP_STRING([--enable-gtk3],
						[compile against GTK3 [default=auto]])],
		[enable_gtk3=$enableval],
		[enable_gtk3=auto])

gtk2_package=gtk+-2.0
gtk2_min_version=2.24
gtk3_package=gtk+-3.0
gtk3_min_version=3.0

PKG_CHECK_EXISTS([$gtk2_package >= $gtk2_min_version], [have_gtk2=yes], [have_gtk2=no])
PKG_CHECK_EXISTS([$gtk3_package >= $gtk3_min_version], [have_gtk3=yes], [have_gtk3=no])
AS_IF([test "x$enable_gtk3" = xyes || (test "x$enable_gtk3" != xno &&
									   test "x$have_gtk3" = xyes &&
									   test "x$have_gtk2" = xno)],
	  [gtk_package=$gtk3_package
	   gtk_min_version=$gtk3_min_version],
	  [gtk_package=$gtk2_package
	   gtk_min_version=$gtk2_min_version])
AM_CONDITIONAL([GTK3], [test "x$gtk_package" = "x$gtk3_package"])

# GTK/GLib/GIO checks
gtk_modules="$gtk_package >= $gtk_min_version glib-2.0 >= 2.32"
gtk_modules_private="gio-2.0 >= 2.32 gmodule-no-export-2.0"
PKG_CHECK_MODULES([GTK], [$gtk_modules $gtk_modules_private])
AC_SUBST([DEPENDENCIES], [$gtk_modules])
AS_VAR_APPEND([GTK_CFLAGS], [" -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32"])
dnl Disable all GTK deprecations on 3.x so long as we want to keep 2.x support and only require 3.0.
dnl No need on 2.x as we target the latest version.
AM_COND_IF([GTK3], [AS_VAR_APPEND([GTK_CFLAGS], [" -DGDK_DISABLE_DEPRECATION_WARNINGS"])])
AC_SUBST([GTK_CFLAGS])
AC_SUBST([GTK_LIBS])
GTK_VERSION=`$PKG_CONFIG --modversion $gtk_package`
AC_SUBST([GTK_VERSION])
GEANY_STATUS_ADD([Using GTK version], [${GTK_VERSION}])
# GTHREAD checks
gthread_modules="gthread-2.0"
PKG_CHECK_MODULES([GTHREAD], [$gthread_modules])
AC_SUBST([GTHREAD_CFLAGS])
AC_SUBST([GTHREAD_LIBS])

# --disable-deprecated switch for GTK purification
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
GEANY_CHECK_MAC_INTEGRATION
GEANY_CHECK_THE_FORCE dnl hehe

# i18n
GEANY_I18N

# double eval since datarootdir is usually defined as ${prefix}/share
AM_COND_IF([MINGW],
  [pkgdatadir='${prefix}/data'],
  [pkgdatadir='${datarootdir}/geany'])
AC_SUBST([GEANY_DATA_DIR], [$(eval echo $(eval echo $pkgdatadir))])
AC_SUBST([pkgdatadir])

# Documentation tools
GEANY_CHECK_DOCUTILS
GEANY_CHECK_DOXYGEN
GEANY_CHECK_GTKDOC_HEADER

# libgeany
GEANY_LIB_INIT

# Output
AC_CONFIG_FILES([
		Makefile
		icons/Makefile
		icons/16x16/Makefile
		icons/24x24/Makefile
		icons/32x32/Makefile
		icons/48x48/Makefile
		icons/scalable/Makefile
		icons/tango/Makefile
		icons/tango/16x16/Makefile
		icons/tango/24x24/Makefile
		icons/tango/32x32/Makefile
		icons/tango/48x48/Makefile
		icons/tango/scalable/Makefile
		ctags/Makefile
		scintilla/Makefile
		scintilla/include/Makefile
		src/Makefile
		src/tagmanager/Makefile
		plugins/Makefile
		po/Makefile.in
		data/Makefile
		doc/Makefile
		doc/geany.1
		geany.pc
		geany.nsi
		doc/Doxyfile
		tests/Makefile
		tests/ctags/Makefile
])
AC_OUTPUT

# Summary
GEANY_STATUS_OUTPUT
echo ""
echo "Configuration is done OK."
echo ""
