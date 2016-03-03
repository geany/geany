AC_DEFUN([_GEANY_CHECK_GTKDOC_HEADER_ERROR],
[
	AC_MSG_ERROR([GtkDoc header generation enabled but $1])
])

dnl GEANY_CHECK_GTKDOC_HEADER
dnl checks for GtkDoc header generation requirements and define
dnl ENABLE_GTKDOC_HEADER Automake conditional as appropriate
AC_DEFUN([GEANY_CHECK_GTKDOC_HEADER],
[
	AC_REQUIRE([GEANY_CHECK_DOXYGEN])

	AC_ARG_ENABLE([gtkdoc-header],
			[AS_HELP_STRING([--enable-gtkdoc-header],
					[generate the GtkDoc header suitable for GObject introspection [default=auto]])],
			[geany_enable_gtkdoc_header="$enableval"],
			[geany_enable_gtkdoc_header="auto"])

	AS_IF([test "x$geany_enable_gtkdoc_header$geany_with_doxygen" = "xyesno"],
	      [_GEANY_CHECK_GTKDOC_HEADER_ERROR([Doxygen support not available])],
	      [test "x$geany_enable_gtkdoc_header" != "xno"],
	[
		dnl python
		AM_PATH_PYTHON([2.7], [have_python=yes], [have_python=no])
		dnl lxml module
		AS_IF([test "x$have_python" = xyes],
		      [have_python_and_lxml=yes
		       AC_MSG_CHECKING([for python lxml package])
		       AS_IF([$PYTHON -c 'import lxml' >/dev/null 2>&1],
		             [have_python_and_lxml=yes],
		             [have_python_and_lxml=no])
		       AC_MSG_RESULT([$have_python_and_lxml])],
		      [have_python_and_lxml=no])
		dnl final result
		AS_IF([test "x$geany_enable_gtkdoc_header$have_python_and_lxml" = "xyesno"],
		      [_GEANY_CHECK_GTKDOC_HEADER_ERROR([python or its lxml module not found])],
		      [geany_enable_gtkdoc_header=$have_python_and_lxml])
	])

	AM_CONDITIONAL([ENABLE_GTKDOC_HEADER], [test "x$geany_enable_gtkdoc_header" = "xyes"])
	GEANY_STATUS_ADD([Generate GtkDoc header], [$geany_enable_gtkdoc_header])
])
