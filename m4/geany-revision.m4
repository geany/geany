dnl GEANY_CHECK_REVISION([action-if-found], [action-if-not-found])
dnl Check for the Git-SVN or SVN revision and set REVISION to
dnl "r<revnum>" or to "-1" if the revision can't be found
dnl Also AC_DEFINEs REVISION
AC_DEFUN([GEANY_CHECK_REVISION],
[
	REVISION="r0"

	AC_MSG_CHECKING([for SVN revision])
	# try Git first
	GIT=`which git 2>/dev/null`
	if test -d ".git" -a "x${GIT}" != "x" -a -x "${GIT}"; then
		# check for git-svn repo first - find-rev (v1.5.4.1) doesn't always fail with git-only repo
		git svn info &>/dev/null
		if test "x$?" == "x0"; then
			REVISION=r`git svn find-rev origin/trunk 2>/dev/null ||
					git svn find-rev trunk 2>/dev/null ||
					git svn find-rev HEAD 2>/dev/null ||
					git svn find-rev master 2>/dev/null ||
					echo 0`
		fi
	fi
	# then check for SVN
	if test "x${REVISION}" = "xr0"; then
		SVN=`which svn 2>/dev/null`
		if test -d ".svn" -a "x${SVN}" != "x" -a -x "${SVN}"; then
			REVISION=r`$SVN info|grep 'Last Changed Rev'|cut -d' ' -f4`
		fi
	fi

	if test "x${REVISION}" != "xr0"; then
		AC_MSG_RESULT([$REVISION])

		# call action-if-found
		$1
	else
		REVISION="-1"
		AC_MSG_RESULT([none])

		# call action-if-not-found
		$2
	fi

	AC_DEFINE_UNQUOTED([REVISION], "$REVISION", [subversion revision number])
])
