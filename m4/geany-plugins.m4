dnl GEANY_CHECK_PLUGINS
dnl Checks whether to enable plugins support
dnl AC_DEFINEs HAVE_PLUGINS and AM_CONDITIONALs PLUGINS
dnl Result is available in the geany_enable_plugins variable
AC_DEFUN([GEANY_CHECK_PLUGINS],
[
	AC_REQUIRE([AC_DISABLE_STATIC])
	AC_REQUIRE([AM_PROG_LIBTOOL])

	AC_ARG_ENABLE([plugins],
			[AS_HELP_STRING([--disable-plugins], [compile without plugin support [default=no]])],
			[geany_enable_plugins=$enableval],
			[geany_enable_plugins=yes])

	dnl silent libtool if it's not already done
	AS_CASE(["$LIBTOOL"],
			[*--silent*], [],
			[LIBTOOL="$LIBTOOL --silent"
			 AC_SUBST([LIBTOOL])])

	if test "x$geany_enable_plugins" = "xyes" ; then
		AC_DEFINE([HAVE_PLUGINS], [1], [Define if plugins are enabled.])
		AM_CONDITIONAL([PLUGINS], true)
	else
		AM_CONDITIONAL([PLUGINS], false)
	fi

	GEANY_STATUS_ADD([Build with plugin support], [$geany_enable_plugins])
])
