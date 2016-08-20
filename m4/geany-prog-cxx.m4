dnl GEANY_PROG_CXX
dnl Check for a working C++ compiler.
dnl like AC_PROG_CXX, but makes sure the compiler actually works instead of
dnl falling back on a reasonable default only.
dnl
dnl You must call AC_PROG_CXX yourself before this macro.
AC_DEFUN([GEANY_PROG_CXX],
[
	AC_REQUIRE([AC_PROG_CXX])

	AC_LANG_PUSH([C++])
	AC_MSG_CHECKING([whether the C++ compiler works])
	AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM([[class Test {public: static int main() {return 0;}};]],
			                 [[Test::main();]])],
			[AC_MSG_RESULT([yes])],
			[AC_MSG_RESULT([no])
			 AC_MSG_ERROR([The C++ compiler $CXX does not work. Please install a working C++ compiler or define CXX to the appropriate value.])])
	AC_LANG_POP([C++])
])
