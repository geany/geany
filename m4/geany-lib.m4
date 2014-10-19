AC_DEFUN([GEANY_LIB_INIT],
[

dnl In the future, if we want to use libtool/library versioning, we can
dnl set these variables accordingly. For now its the same as if not specified (0:0:0)
	libgeany_current=0
	libgeany_revision=0
	libgeany_age=0

dnl Try and see if we can use -fvisibility compiler option and GCC`s
dnl `__attribute__((visibility(...)))` extension and use it if so.
	AC_MSG_CHECKING([whether compiler supports -fvisibility])
	libgeany_backup_cflags=$CFLAGS
	CFLAGS=-fvisibility=hidden
	AC_TRY_COMPILE([], [
			__attribute__ ((visibility ("default")))
			int main(int argc, char **argv) { return 0; }
		], [
			LIBGEANY_CFLAGS="${CFLAGS}"
			AC_MSG_RESULT([yes])
		], [
			LIBGEANY_CFLAGS=""
			AC_MSG_RESULT([no])
		])
	CFLAGS="${libgeany_backup_cflags}"

	LIBGEANY_LIBS="-version-info ${libgeany_current}:${libgeany_revision}:${libgeany_age}"

	AC_SUBST([LIBGEANY_CFLAGS])
	AC_SUBST([LIBGEANY_LIBS])

dnl Check for utilities needed to do codegen
	AC_PATH_PROG([SORT], [sort], [
		AC_MSG_ERROR([The 'sort' utility is required, is it installed?])])
	AC_PATH_PROG([UNIQ], [uniq], [
		AC_MSG_ERROR([The 'uniq' utility is required, is it installed?])])
])
