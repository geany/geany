dnl GEANY_CHECK_MAC_INTEGRATION
dnl Check for gtk-mac-integration to enable improved OS X integration
dnl
AC_DEFUN([GEANY_CHECK_MAC_INTEGRATION],
[
	AC_ARG_ENABLE([mac-integration],
			[AS_HELP_STRING([--enable-mac-integration],
					[use gtk-mac-integration to enable improved OS X integration [default=no]])],
			[geany_enable_mac_integration="$enableval"],
			[geany_enable_mac_integration="no"])

	AM_CONDITIONAL(ENABLE_MAC_INTEGRATION, [test "x$geany_enable_mac_integration" = "xyes"])
	AM_COND_IF(ENABLE_MAC_INTEGRATION,
	[
		AS_IF([test "x$enable_gtk3" = xyes],
			[PKG_CHECK_MODULES(MAC_INTEGRATION, gtk-mac-integration-gtk3)], 
			[PKG_CHECK_MODULES(MAC_INTEGRATION, gtk-mac-integration-gtk2)])
	])
])
