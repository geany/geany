dnl GEANY_CHECK_MINGW
dnl Checks whether we're building for MinGW, and defines appropriate stuff
dnl if it is the case.
dnl Most importantly, AM_CODITIONALs MINGW
AC_DEFUN([GEANY_CHECK_MINGW],
[
	case "${host}" in
		*mingw*)
			AC_CHECK_LIB([iberty], [fnmatch], [],
					[AC_MSG_ERROR([fnmatch does not present in libiberty. You need to update it, read http://www.geany.org/Support/CrossCompile for details.])])
			AC_DEFINE([WIN32], [1], [we are cross compiling for WIN32])
			GEANY_CHECK_VTE([no])
			GEANY_CHECK_SOCKET([yes])
			AM_CONDITIONAL([MINGW], true)
			;;
		*)
			AM_CONDITIONAL([MINGW], false)
			;;
	esac
])
