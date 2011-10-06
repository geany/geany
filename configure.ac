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
AC_PROG_INTLTOOL

# for plugins
AC_DISABLE_STATIC
AM_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"

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
AC_ARG_ENABLE([plugins],
		[AS_HELP_STRING([--disable-plugins], [compile without plugin support  [default=no]])],
		,
		[enable_plugins=yes])

if test "x$enable_plugins" = "xyes" ; then
	AC_DEFINE([HAVE_PLUGINS], [1], [Define if plugins are enabled.])
	AM_CONDITIONAL([PLUGINS], true)
else
	AM_CONDITIONAL([PLUGINS], false)
fi


# check whether to use included GNU regex library
AC_ARG_ENABLE([gnu-regex],
		[AS_HELP_STRING([--enable-gnu-regex],
				[compile with included GNU regex library [default=no]])],
		,
		[enable_gnu_regex=no])

# auto-enable included regex if necessary
# FIXME: this may override a user choice
AC_CHECK_FUNCS([regcomp], [], [enable_gnu_regex="yes"])

if test "x$enable_gnu_regex" = "xyes" ; then
	AC_DEFINE([USE_INCLUDED_REGEX], [1], [Define if included GNU regex code should be used.])
	AC_DEFINE([HAVE_REGCOMP], [1], [Define if you have the 'regcomp' function.])
	AM_CONDITIONAL([USE_INCLUDED_REGEX], true)
else
	AM_CONDITIONAL([USE_INCLUDED_REGEX], false)
fi


# check for mingw specific settings
case "${host}" in
	*mingw*)
		AC_CHECK_LIB([iberty], [fnmatch], [],
				[AC_MSG_ERROR([fnmatch does not present in libiberty. You need to update it, read http://www.geany.org/Support/CrossCompile for details.])])
		AC_DEFINE([WIN32], [1], [we are cross compiling for WIN32])
		AC_DEFINE([HAVE_SOCKET], [1], [Define if you want to detect a running instance])
		AM_CONDITIONAL([MINGW], true)
		want_vte="no"
		want_socket="yes"
		;;
	*)
		AM_CONDITIONAL([MINGW], false)
		;;
esac


# skip check if already decided
if test "x$want_socket" = "x"; then
	# socket support
	AC_ARG_ENABLE([socket],
			[AS_HELP_STRING([--enable-socket],
					[enable if you want to detect a running instance [default=yes]])],
			[want_socket="$enableval"],
			[want_socket="yes"])

	if test "x$want_socket" = "xyes"; then
		AC_DEFINE([HAVE_SOCKET], [1], [Define if you want to detect a running instance])
		# this should bring in libsocket on Solaris:
		AC_SEARCH_LIBS([connect],[socket])
	fi
fi


# skip check if already decided
if test "x$want_vte" = "x"; then
	# VTE support
	AC_ARG_ENABLE([vte],
			[AS_HELP_STRING([--enable-vte],
					[enable if you want virtual terminal support [default=yes]])],
			[want_vte="$enableval"],
			[want_vte="yes"])
	if test "x$want_vte" = "xyes"; then
		AC_DEFINE([HAVE_VTE], [1], [Define if you want VTE support])
	fi
	AC_ARG_WITH([vte-module-path],
			[AS_HELP_STRING([--with-vte-module-path=PATH],
					[Path to a loadable libvte [default=None]])],
			[AC_DEFINE_UNQUOTED([VTE_MODULE_PATH],
					["$withval"], [Path to a loadable libvte])])
fi


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
GETTEXT_PACKAGE="$PACKAGE"
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext package.])

if test -n "${LINGUAS}"; then
	ALL_LINGUAS="${LINGUAS}"
else
	if test -z "$conf_dir"; then
		conf_dir="."
	fi
	ALL_LINGUAS=`cd "$conf_dir/po" 2>/dev/null && ls *.po 2>/dev/null | $AWK 'BEGIN { FS="."; ORS=" " } { print $1 }'`
fi

AM_GLIB_GNU_GETTEXT
# workaround for intltool bug (http://bugzilla.gnome.org/show_bug.cgi?id=490845)
if test "x$MSGFMT" = "xno"; then
	AC_MSG_ERROR([msgfmt not found. Please install the gettext package.])
fi


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

# intltool hack to define install_sh on Debian/Ubuntu systems
if test "x$install_sh" = "x"; then
	install_sh="`pwd`/install-sh"
	AC_SUBST([install_sh])
fi


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
echo "Build with plugin support          : ${enable_plugins}"
echo "Use virtual terminal support       : ${want_vte}"
echo "Use (UNIX domain) socket support   : ${want_socket}"
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
