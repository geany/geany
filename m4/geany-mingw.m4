dnl GEANY_CHECK_MINGW
dnl Checks whether we're building for MinGW, and defines appropriate stuff
dnl if it is the case.
dnl Most importantly, AM_CODITIONALs MINGW
AC_DEFUN([GEANY_CHECK_MINGW],
[
	case "${host}" in
		*mingw*)
			GEANY_CHECK_VTE([no])
			GEANY_CHECK_SOCKET([yes])
			AM_CONDITIONAL([MINGW], true)
			;;
		*)
			AM_CONDITIONAL([MINGW], false)
			;;
	esac
])
