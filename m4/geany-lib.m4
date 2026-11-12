dnl checks whether the compiler supports GCC4-style visibility
AC_DEFUN([GCC4_VISIBILITY],
[
	have_gcc4_visibility=no
	AC_MSG_CHECKING([whether compiler supports GCC4-style visibility])
	gcc_visibility_backup_cflags=$CFLAGS
	CFLAGS=-fvisibility=hidden
	AC_LINK_IFELSE([AC_LANG_SOURCE([[__attribute__((visibility("default")))
									 int main(int argc, char **argv) { return 0; }]])],
				   [have_gcc4_visibility=yes])
	AC_MSG_RESULT([$have_gcc4_visibility])
	CFLAGS="${gcc_visibility_backup_cflags}"
])

dnl Checks and fills LIBGEANY_EXPORT_CFLAGS
AC_DEFUN([GEANY_EXPORT],
[
	AC_REQUIRE([GEANY_CHECK_MINGW])
	AC_REQUIRE([GCC4_VISIBILITY])

	dnl FIXME: better way to detect Windows?
	AM_COND_IF([MINGW], [win32=yes], [win32=false])

	dnl Clang rejects adding dllexport on a redeclaration once the function has
	dnl been used, and GEANY_API_SYMBOL is only put on definitions, not on the
	dnl header declarations. So with Clang on Windows, let the linker export
	dnl libgeany's symbols instead.
	win32_clang=no
	AS_IF([test x$win32 = xyes],
		  [AC_MSG_CHECKING([whether the compiler is Clang])
		   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#ifndef __clang__
#error not clang
#endif]])], [win32_clang=yes])
		   AC_MSG_RESULT([$win32_clang])])

	export_CFLAGS=
	export_LDFLAGS=
	AS_IF([test x$win32_clang = xyes],
		  [export_CFLAGS='-DGEANY_EXPORT_SYMBOL='
		   export_LDFLAGS='-Wl,--export-all-symbols'],
		  [test x$win32 = xyes],
		  [export_CFLAGS='-DGEANY_EXPORT_SYMBOL="__declspec(dllexport)"'],
		  [test x$have_gcc4_visibility = xyes],
		  [export_CFLAGS='-fvisibility=hidden -DGEANY_EXPORT_SYMBOL="__attribute__((visibility(\"default\")))"'],
		  [dnl GEANY_EXPORT_SYMBOL expands to nothing
		   export_CFLAGS='-DGEANY_EXPORT_SYMBOL'])

	LIBGEANY_EXPORT_CFLAGS="${export_CFLAGS} -DGEANY_API_SYMBOL=GEANY_EXPORT_SYMBOL"
	LIBGEANY_EXPORT_LDFLAGS="${export_LDFLAGS}"

	AC_SUBST([LIBGEANY_EXPORT_CFLAGS])
	AC_SUBST([LIBGEANY_EXPORT_LDFLAGS])
])

AC_DEFUN([GEANY_LIB_INIT],
[
	AC_REQUIRE([GEANY_EXPORT])

dnl In the future, if we want to use libtool/library versioning, we can
dnl set these variables accordingly. For now its the same as if not specified (0:0:0)
	libgeany_current=0
	libgeany_revision=0
	libgeany_age=0

	LIBGEANY_CFLAGS="${LIBGEANY_EXPORT_CFLAGS}"
	LIBGEANY_LDFLAGS="-version-info ${libgeany_current}:${libgeany_revision}:${libgeany_age} ${LIBGEANY_EXPORT_LDFLAGS}"

	AC_SUBST([LIBGEANY_CFLAGS])
	AC_SUBST([LIBGEANY_LDFLAGS])

dnl Check for utilities needed to do codegen
	AC_PATH_PROG([SORT], [sort], [
		AC_MSG_ERROR([The 'sort' utility is required, is it installed?])])
	AC_PATH_PROG([UNIQ], [uniq], [
		AC_MSG_ERROR([The 'uniq' utility is required, is it installed?])])
])
