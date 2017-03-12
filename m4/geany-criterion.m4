dnl Checks for everything required for criterion unit tests
AC_DEFUN([GEANY_CHECK_CRITERION],
[
	AC_REQUIRE_AUX_FILE([tap-driver.sh])
	AC_PROG_AWK

	PKG_CHECK_MODULES([CRITERION], [criterion >= 2.3.1],
			[AM_CONDITIONAL([WITH_CRITERION], true)],
			[AM_CONDITIONAL([WITH_CRITERION], false)])
])
