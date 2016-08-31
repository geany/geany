dnl GEANY_PREFIX
dnl Ensures $prefix and $exec_prefix are set to something sensible
dnl
dnl Logic taken from Geany-Plugins' build/expansions.m4
AC_DEFUN([GEANY_PREFIX],
[
	if test "x$prefix" = xNONE; then
		prefix=$ac_default_prefix
	fi

	if test "x$exec_prefix" = xNONE; then
		exec_prefix=$prefix
	fi
])

dnl GEANY_DOCDIR
dnl Ensures $docdir is set and AC_SUBSTed
dnl
dnl FIXME: is this really useful?
AC_DEFUN([GEANY_DOCDIR],
[
	if test -z "${docdir}"; then
		docdir='${datadir}/doc/${PACKAGE}'
		AC_SUBST([docdir])
	fi
])
