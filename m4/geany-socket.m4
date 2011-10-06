dnl _GEANY_CHECK_SOCKET_PREREQ
AC_DEFUN([_GEANY_CHECK_SOCKET_PREREQ],
[
	AC_ARG_ENABLE([socket],
			[AS_HELP_STRING([--enable-socket],
					[enable if you want to detect a running instance [default=yes]])],
			[geany_enable_socket="$enableval"],
			[geany_enable_socket="auto"])
])

dnl GEANY_CHECK_SOCKET([enable])
AC_DEFUN([GEANY_CHECK_SOCKET],
[
	AC_REQUIRE([_GEANY_CHECK_SOCKET_PREREQ])

	dnl this way of calling once is a bit ugly, but we need to be able to
	dnl call this from one or more locations, the first one maybe in a shell
	dnl conditional.
	if test "x$_geany_enable_socket_done" = x; then
		dnl This one gives precedence for user choice
		dnl if test "x$geany_enable_socket" = xauto; then
		dnl 	if test -n "$1"; then
		dnl 		geany_enable_socket="$1"
		dnl 	else
		dnl 		geany_enable_socket=yes
		dnl 	fi
		dnl fi
		if test -n "$1"; then
			geany_enable_socket="$1"
		elif test "x$geany_enable_socket" = xauto; then
			geany_enable_socket=yes
		fi

		if test "x$geany_enable_socket" = xyes; then
			AC_DEFINE([HAVE_SOCKET], [1], [Define if you want to detect a running instance])
			# this should bring in libsocket on Solaris:
			AC_SEARCH_LIBS([connect],[socket])
		fi

		GEANY_STATUS_ADD([Use (UNIX domain) socket support], [$geany_enable_socket])
		_geany_enable_socket_done=yes
	fi
])
