dnl GEANY_CHECK_DOXYGEN
dnl Check for Doxygen availability to generate API docs
dnl
AC_DEFUN([GEANY_CHECK_DOXYGEN],
[
	AC_ARG_ENABLE([api-docs],
			[AS_HELP_STRING([--enable-api-docs],
					[generate API documentation using Doxygen [default=no]])],
			[geany_with_doxygen="$enableval"],
			[geany_with_doxygen="auto"])

	AC_ARG_VAR([DOXYGEN], [Path to Doxygen executable])

	AS_IF([test "x$geany_with_doxygen" != "xno"],
	[
		AC_PATH_PROG([DOXYGEN], [doxygen], [no])
		AS_IF([test "x$DOXYGEN" != "xno"],
			  [geany_with_doxygen=yes],
			  [test "x$geany_with_doxygen" = xyes],
			  [AC_MSG_ERROR([API documentation enabled but doxygen not found])],
			  [geany_with_doxygen=no])
	])

	AM_CONDITIONAL([WITH_DOXYGEN], [test "x$geany_with_doxygen" != "xno"])
	GEANY_STATUS_ADD([Build API documentation], [$geany_with_doxygen])
])
