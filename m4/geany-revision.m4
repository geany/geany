dnl GEANY_CHECK_REVISION([action-if-found], [action-if-not-found])
dnl Check for the Git-SVN or SVN revision and set REVISION to
dnl "r<revnum>" or to "-1" if the revision can't be found
dnl Also AC_DEFINEs REVISION
AC_DEFUN([GEANY_CHECK_REVISION],
[
	REVISION="0"

	AC_MSG_CHECKING([for Git revision])
	# try Git first
	GIT=`which git 2>/dev/null`
	if test -d ".git" -a "x${GIT}" != "x" -a -x "${GIT}"; then
		REVISION=`git rev-parse --short --revs-only HEAD || 0`
	fi

	if test "x${REVISION}" != "x0"; then
		AC_MSG_RESULT([$REVISION])
		GEANY_STATUS_ADD([Compiling Git revision], [$REVISION])

		# call action-if-found
		$1
	else
		REVISION="-1"
		AC_MSG_RESULT([none])

		# call action-if-not-found
		$2
	fi

	AC_DEFINE_UNQUOTED([REVISION], "$REVISION", [git revision hash])
])
