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

	# The $bindir variable is equal to the literal string "${exec_dir}/bin"
	# rather than the actual path. pkgconfig (.pc) files can use that ok, but
	# other files written out by configure require the literal path.
	#
	# If the --bindir argument is given to configure, then $bindir will already
	# be equal to an absolute path.
	BINDIR=$bindir
	if test "x$BINDIR" = "x\${exec_prefix}/bin"; then
		BINDIR=${exec_prefix}/bin
	fi
	AC_SUBST([BINDIR])
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
