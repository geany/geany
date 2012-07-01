dnl GEANY_I18N
dnl Setups I18N support.
dnl AC_DEFINEs and AC_SUBSTs GETTEXT_PACKAGE
AC_DEFUN([GEANY_I18N],
[
	AC_REQUIRE([AC_PROG_AWK])
	AC_REQUIRE([AC_PROG_INTLTOOL])

	GETTEXT_PACKAGE="$PACKAGE"
	AC_SUBST([GETTEXT_PACKAGE])
	AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext package.])

	if test -n "${LINGUAS}"; then
		ALL_LINGUAS="${LINGUAS}"
	else
		ALL_LINGUAS=`cd "$srcdir/po" 2>/dev/null && ls *.po 2>/dev/null | $AWK 'BEGIN { FS="."; ORS=" " } { print $[]1 }'`
	fi

	AM_GLIB_GNU_GETTEXT
	# workaround for intltool bug (http://bugzilla.gnome.org/show_bug.cgi?id=490845)
	if test "x$MSGFMT" = "xno"; then
		AC_MSG_ERROR([msgfmt not found. Please install the gettext package.])
	fi

	# intltool hack to define install_sh on Debian/Ubuntu systems
	if test "x$install_sh" = "x"; then
		install_sh="`pwd`/install-sh"
		AC_SUBST([install_sh])
	fi
])
