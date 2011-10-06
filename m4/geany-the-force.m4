dnl GEANY_CHECK_THE_FORCE
dnl just for a laugh (it has absolutely no effect)
AC_DEFUN([GEANY_CHECK_THE_FORCE],
[
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
])
