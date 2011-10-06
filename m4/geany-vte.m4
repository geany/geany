dnl _GEANY_CHECK_VTE_PREREQ
AC_DEFUN([_GEANY_CHECK_VTE_PREREQ],
[
	AC_ARG_ENABLE([vte],
			[AS_HELP_STRING([--enable-vte],
					[enable if you want virtual terminal support [default=yes]])],
			[geany_enable_vte="$enableval"],
			[geany_enable_vte="yes"])
	AC_ARG_WITH([vte-module-path],
			[AS_HELP_STRING([--with-vte-module-path=PATH],
					[Path to a loadable libvte [default=None]])],
			[AC_DEFINE_UNQUOTED([VTE_MODULE_PATH],
					["$withval"], [Path to a loadable libvte])])
])

dnl GEANY_CHECK_VTE([enable])
AC_DEFUN([GEANY_CHECK_VTE],
[
	AC_REQUIRE([_GEANY_CHECK_VTE_PREREQ])

	dnl this way of calling once is a bit ugly, but we need to be able to
	dnl call this from one or more locations, the first one maybe in a shell
	dnl conditional.
	dnl see geany-socket.m4
	if test "x$_geany_enable_vte_done" = x; then
		dnl This one gives precedence for user choice
		dnl if test "x$geany_enable_vte" = xauto; then
		dnl 	if test -n "$1"; then
		dnl 		geany_enable_vte="$1"
		dnl 	else
		dnl 		geany_enable_vte=yes
		dnl 	fi
		dnl fi
		if test -n "$1"; then
			geany_enable_vte="$1"
		elif test "x$geany_enable_vte" = xauto; then
			geany_enable_vte=yes
		fi

		if test "x$geany_enable_vte" = xyes; then
			AC_DEFINE([HAVE_VTE], [1], [Define if you want VTE support])
		fi

		GEANY_STATUS_ADD([Use virtual terminal support], [$geany_enable_vte])
		_geany_enable_vte_done=yes
	fi
])
