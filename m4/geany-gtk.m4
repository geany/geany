dnl GEANY_CHECK_GTK
dnl Checks whether the GTK stack is available and new enough. Sets GTK_CFLAGS and GTK_LIBS.
AC_DEFUN([GEANY_CHECK_GTK],
[
	gtk_modules="gtk+-3.0 >= 3.24 glib-2.0 >= 2.32"
	gtk_modules_private="gio-2.0 >= 2.32 gmodule-no-export-2.0 gthread-2.0"

	PKG_CHECK_MODULES([GTK], [$gtk_modules $gtk_modules_private])
	AC_SUBST([DEPENDENCIES], [$gtk_modules])
	AS_VAR_APPEND([GTK_CFLAGS], [" -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32"])
	dnl Disable all GTK deprecations
	AS_VAR_APPEND([GTK_CFLAGS], [" -DGDK_DISABLE_DEPRECATION_WARNINGS"])
	AC_SUBST([GTK_CFLAGS])
	AC_SUBST([GTK_LIBS])
	AC_SUBST([GTK_VERSION],[`$PKG_CONFIG --modversion gtk+-3.0`])

	GEANY_STATUS_ADD([Using GTK version], [${GTK_VERSION}])
])

dnl GEANY_CHECK_GTK_FUNCS
dnl Like AC_CHECK_FUNCS but adds GTK flags so that tests for GLib/GTK functions may succeed.
AC_DEFUN([GEANY_CHECK_GTK_FUNCS],
[
	AC_REQUIRE([GEANY_CHECK_GTK])

	CFLAGS_save=$CFLAGS
	CFLAGS=$GTK_CFLAGS
	LIBS_save=$LIBS
	LIBS=$GTK_LIBS
	AC_CHECK_FUNCS([$1])
	CFLAGS=$CFLAGS_save
	LIBS=$LIBS_save
])
