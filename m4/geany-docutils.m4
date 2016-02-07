dnl GEANY_CHECK_DOCUTILS
dnl Check for the tools used to generate documentation
dnl
AC_DEFUN([GEANY_CHECK_DOCUTILS],
[
	GEANY_CHECK_DOCUTILS_HTML
	GEANY_CHECK_DOCUTILS_PDF
])
dnl
dnl GEANY_CHECK_DOCUTILS_HTML
dnl For HTML documentation generation
dnl
AC_DEFUN([GEANY_CHECK_DOCUTILS_HTML],
[
	AC_REQUIRE([GEANY_CHECK_REVISION])

	AS_IF([test -f "$srcdir/doc/geany.html"],
		[have_prebuilt_html_docs=yes],
		[have_prebuilt_html_docs=no])

	dnl we require rst2html by default unless we don't build from Git
	dnl and already have the HTML manual built in-tree
	html_docs_default=yes
	AS_IF([test "$REVISION" = "-1" && test "x$have_prebuilt_html_docs" = xyes],
		[html_docs_default=auto])

	AC_ARG_ENABLE([html-docs],
		[AS_HELP_STRING([--enable-html-docs],
			[generate HTML documentation using rst2html [default=auto]])],
		[geany_enable_html_docs="$enableval"],
		[geany_enable_html_docs="$html_docs_default"])
	AC_ARG_VAR([RST2HTML], [Path to Docutils rst2html executable])
	AS_IF([test "x$geany_enable_html_docs" != "xno"],
	[
		AC_PATH_PROGS([RST2HTML], [rst2html rst2html.py], [no])
		AS_IF([test "x$RST2HTML" != "xno"],
			[geany_enable_html_docs="yes"],
			[test "x$geany_enable_html_docs" = "xyes"],
			[AC_MSG_ERROR([Documentation enabled but rst2html not found.
You can explicitly disable building of the HTML manual with --disable-html-docs,
but you then may not have a local copy of the HTML manual.])],
			[geany_enable_html_docs="no"])
	])
	AM_CONDITIONAL([WITH_RST2HTML], [test "x$geany_enable_html_docs" != "xno"])
	AM_CONDITIONAL([INSTALL_HTML_DOCS], [test "x$geany_enable_html_docs" != "xno" ||
	                                     test "x$have_prebuilt_html_docs" = xyes])
	GEANY_STATUS_ADD([Build HTML documentation], [$geany_enable_html_docs])
])
dnl
dnl GEANY_CHECK_DOCUTILS_PDF
dnl For PDF documentation generation
dnl
AC_DEFUN([GEANY_CHECK_DOCUTILS_PDF],
[
	AC_ARG_ENABLE([pdf-docs],
		[AS_HELP_STRING([--enable-pdf-docs],
			[generate PDF documentation using rst2pdf [default=auto]])],
		[geany_enable_pdf_docs="$enableval"],
		[geany_enable_pdf_docs="auto"])
	AC_ARG_VAR([RST2PDF], [Path to Docutils rst2pdf executable])
	AS_IF([test "x$geany_enable_pdf_docs" != "xno"],
	[
		AC_PATH_PROGS([RST2PDF], [rst2pdf rst2pdf.py], [no])
		AS_IF([test "x$RST2PDF" != "xno"],
			[geany_enable_pdf_docs="yes"],
			[test "x$geany_enable_pdf_docs" = "xyes"],
			[AC_MSG_ERROR([PDF documentation enabled but rst2pdf not found])],
			[geany_enable_pdf_docs="no"])
	])
	AM_CONDITIONAL([WITH_RST2PDF], [test "x$geany_enable_pdf_docs" != "xno"])
	GEANY_STATUS_ADD([Build PDF documentation], [$geany_enable_pdf_docs])
])

dnl
dnl GEANY_CHECK_PYTHON
dnl For gtkdoc header generation
dnl
AC_DEFUN([GEANY_CHECK_PYTHON],
[
	AM_PATH_PYTHON([2.7], [], [])

	have_python=no
	AS_IF([test -n "$PYTHON"], [
		AC_MSG_CHECKING([for python lxml package])
		$PYTHON -c 'import lxml' 1>&2 2>/dev/null
		AS_IF([test $? -eq 0], [
			AC_MSG_RESULT([found])
			have_python=yes
		], [
			AC_MSG_RESULT([not found])
			have_python=no
		])
	])

	AM_CONDITIONAL([WITH_PYTHON], [test "x$have_python" = "xyes"])
	AM_COND_IF([WITH_PYTHON],
		[GEANY_STATUS_ADD([Using Python version], [$PYTHON_VERSION])])
])
