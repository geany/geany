dnl GEANY_CHECK_GNU_REGEX
dnl Checks whether to use internal GNU regex library
dnl Defines USE_INCLUDED_REGEX both with AC_DEFINE and as an
dnl AM_CONDITIONAL
AC_DEFUN([GEANY_CHECK_GNU_REGEX],
[
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
		GEANY_STATUS_ADD([GNU regex library], [built-in])
	else
		AM_CONDITIONAL([USE_INCLUDED_REGEX], false)
		GEANY_STATUS_ADD([GNU regex library], [system])
	fi
])
