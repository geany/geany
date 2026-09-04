dnl GEANY_CHECK_GTK
dnl Checks whether the GTK stack is available and new enough. Sets GTK_CFLAGS and GTK_LIBS.
AC_DEFUN([GEANY_CHECK_GTK],
[
	gtk_modules="gtk+-3.0 >= 3.24 glib-2.0 >= 2.56"
	gtk_modules_private="gio-2.0 >= 2.56 gmodule-no-export-2.0 gthread-2.0"

	PKG_CHECK_MODULES([GTK], [$gtk_modules $gtk_modules_private])
	AC_SUBST([DEPENDENCIES], [$gtk_modules])
	AC_ARG_ENABLE([deprecation-warnings],
		[AS_HELP_STRING([--enable-deprecation-warnings],
			[Warn about uses of deprecated GTK and GLib API (silenced by default)])],
		[], [enable_deprecation_warnings=no])
	AS_IF([test "x$enable_deprecation_warnings" = "xyes"],
		[dnl Warn about everything deprecated up to the GTK version we require and, by
		 dnl leaving GLIB_VERSION_MIN_REQUIRED at its default, about all GLib
		 dnl deprecations; used to track the remaining work towards GTK 4, see HACKING
		 AS_VAR_APPEND([GTK_CFLAGS], [" -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_24"])],
		[AS_VAR_APPEND([GTK_CFLAGS], [" -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_56"])
		 dnl Disable all GTK deprecations
		 AS_VAR_APPEND([GTK_CFLAGS], [" -DGDK_DISABLE_DEPRECATION_WARNINGS"])])
	AC_SUBST([GTK_CFLAGS])
	AC_SUBST([GTK_LIBS])
	AC_SUBST([GTK_VERSION],[`$PKG_CONFIG --modversion gtk+-3.0`])

	GEANY_STATUS_ADD([Using GTK version], [${GTK_VERSION}])
	GEANY_STATUS_ADD([Warn about deprecated GTK/GLib API], [$enable_deprecation_warnings])
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
