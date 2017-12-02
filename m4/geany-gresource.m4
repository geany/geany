AC_DEFUN([GEANY_CHECK_GRESOURCE],
[
	AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources], [no])
	AS_IF([test "x$GLIB_COMPILE_RESOURCES" = "xno"], [
		AC_MSG_ERROR([Unable to find glib-compile-resources, is it installed?])
	])
])
