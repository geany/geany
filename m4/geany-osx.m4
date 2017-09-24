dnl GEANY_CHECK_OSX
dnl Checks whether we're building for OSX, and defines appropriate stuff
dnl if it is the case.
dnl Most importantly, AM_CODITIONALs OSX
AC_DEFUN([GEANY_CHECK_OSX],
[
	AC_REQUIRE([AC_CANONICAL_HOST])

	AC_MSG_CHECKING([whether we are building for OSX])
	AS_CASE(["${host}"], [*-darwin*], [host_is_osx=yes], [host_is_osx=no])
	AC_MSG_RESULT([$host_is_osx])

	AM_CONDITIONAL([OSX], [test x$host_is_osx = xyes])
])
