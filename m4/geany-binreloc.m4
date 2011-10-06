dnl GEANY_CHECK_BINRELOC
dnl Check for binary relocation support
dnl
dnl logic taken from Inkscape (Hongli Lai <h.lai@chello.nl>)
AC_DEFUN([GEANY_CHECK_BINRELOC],
[
	AC_ARG_ENABLE([binreloc],
			[AS_HELP_STRING([--enable-binreloc],
					[compile with binary relocation support [default=no]])],
			[enable_binreloc=$enableval],
			[enable_binreloc=no])

	AC_MSG_CHECKING([whether binary relocation support should be enabled])
	if test "$enable_binreloc" = "yes"; then
		AC_MSG_RESULT([yes])
		AC_MSG_CHECKING([for linker mappings at /proc/self/maps])
		if test -e /proc/self/maps; then
			AC_MSG_RESULT([yes])
		else
			AC_MSG_RESULT([no])
			AC_MSG_ERROR([/proc/self/maps is not available. Binary relocation cannot be enabled.])
			enable_binreloc="no"
		fi

	elif test "$enable_binreloc" = "auto"; then
		AC_MSG_RESULT([yes when available])
		AC_MSG_CHECKING([for linker mappings at /proc/self/maps])
		if test -e /proc/self/maps; then
			AC_MSG_RESULT([yes])
			enable_binreloc=yes

			AC_MSG_CHECKING([whether everything is installed to the same prefix])
			if test "$bindir" = '${exec_prefix}/bin' -a "$sbindir" = '${exec_prefix}/sbin' -a \
				"$datadir" = '${prefix}/share' -a "$libdir" = '${exec_prefix}/lib' -a \
				"$libexecdir" = '${exec_prefix}/libexec' -a "$sysconfdir" = '${prefix}/etc'
			then
				AC_MSG_RESULT([yes])
			else
				AC_MSG_RESULT([no])
				AC_MSG_NOTICE([Binary relocation support will be disabled.])
				enable_binreloc=no
			fi

		else
			AC_MSG_RESULT([no])
			enable_binreloc=no
		fi

	elif test "$enable_binreloc" = "no"; then
		AC_MSG_RESULT([no])
	else
		AC_MSG_RESULT([no (unknown value "$enable_binreloc")])
		enable_binreloc=no
	fi
	if test "$enable_binreloc" = "yes"; then
		AC_DEFINE([ENABLE_BINRELOC],,[Use AutoPackage?])
	fi

	GEANY_STATUS_ADD([Enable binary relocation], [$enable_binreloc])
])
