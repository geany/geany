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
	AC_ARG_ENABLE([html-docs],
		[AS_HELP_STRING([--enable-html-docs],
			[generate HTML documentation using rst2html [default=no]])],
		[geany_enable_html_docs="$enableval"],
		[geany_enable_html_docs="no"])
	AC_ARG_VAR([RST2HTML], [Path to Docutils rst2html executable])
	AS_IF([test "x$geany_enable_html_docs" != "xno"],
	[
dnl TODO: try rst2html.py first
		AS_IF([test -z "$RST2HTML"], [RST2HTML="rst2html"])
		AC_PATH_PROG([RST2HTML], [$RST2HTML], [no])
		AS_IF([test "x$RST2HTML" != "xno"],
			[geany_enable_html_docs="yes"],
			[test "x$geany_enable_html_docs" = "xyes"],
			[AC_MSG_ERROR([Documentation enabled but rst2html not found])],
			[geany_enable_html_docs="no"])
	])
	AM_CONDITIONAL([WITH_RST2HTML], [test "x$geany_enable_html_docs" != "xno"])
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
			[generate PDF documentation using rst2latex and pdflatex [default=no]])],
		[geany_enable_pdf_docs="$enableval"],
		[geany_enable_pdf_docs="no"])
	AC_ARG_VAR([RST2LATEX], [Path to Docutils rst2latex executable])
	AC_ARG_VAR([PDFLATEX], [Path to pdflatex executable])
	AS_IF([test "x$geany_enable_pdf_docs" != "xno"],
	[
		AS_IF([test -z "$RST2LATEX"], [RST2LATEX="rst2latex"])
		AC_PATH_PROG([RST2LATEX], [$RST2LATEX], [no])
		AS_IF([test -z "$PDFLATEX"], [PDFLATEX="pdflatex"])
		AC_PATH_PROG([PDFLATEX], [$PDFLATEX], [no])
		AS_IF([test "x$RST2LATEX" != "xno" -a "x$PDFLATEX" != "xo"],
			[geany_enable_pdf_docs="yes"],
			[test "x$geany_enable_pdf_docs" = "xyes"],
			[AC_MSG_ERROR([PDF documentation enabled but rst2latex or pdflatex not found])],
			[geany_enable_pdf_docs="no"])
	])
	AM_CONDITIONAL([WITH_LATEXPDF], [test "x$geany_enable_pdf_docs" != "xno"])
	GEANY_STATUS_ADD([Build PDF documentation], [$geany_enable_pdf_docs])
])
