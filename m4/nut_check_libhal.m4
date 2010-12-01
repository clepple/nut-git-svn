dnl Check for LIBHAL compiler flags. On success, set nut_have_libhal="yes"
dnl and set LIBHAL_CFLAGS and LIBHAL_LDFLAGS. On failure, set
dnl nut_have_libhal="no". This macro can be run multiple times, but will
dnl do the checking only once. 
dnl NUT requires HAL version 0.5.8 at least

AC_DEFUN([NUT_CHECK_LIBHAL], 
[
if test -z "${nut_have_libhal_seen}"; then
	nut_have_libhal_seen=yes

	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	AC_MSG_CHECKING(for libhal version via pkg-config (0.5.8 minimum required))
	HAL_VERSION="`pkg-config --silence-errors --modversion hal 2>/dev/null`"
	if test "$?" != "0" -o -z "${HAL_VERSION}"; then
		AC_MSG_RESULT(none found)
	elif pkg-config --silence-errors --atleast-version=0.5.8 hal 2>/dev/null; then
 		AC_MSG_RESULT(${HAL_VERSION} found)
	else
		AC_MSG_WARN(${HAL_VERSION} is too old)
	fi

	AC_MSG_CHECKING(for libhal cflags)
	AC_ARG_WITH(hal-includes,
		AS_HELP_STRING([@<:@--with-hal-includes=CFLAGS@:>@], [include flags for the HAL library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-includes - see docs/configure.txt)
			;;
		*)
			CFLAGS="${withval}"
			;;
		esac
	], [
		dnl also get cflags from glib-2.0 to workaround a bug in dbus-glib
		CFLAGS="`pkg-config --silence-errors --cflags hal dbus-glib-1 2>/dev/null`"
		if test "$?" != "0"; then
			CFLAGS="-DDBUS_API_SUBJECT_TO_CHANGE -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include"
		fi
	])
	AC_MSG_RESULT([${CFLAGS}])

	AC_MSG_CHECKING(for libhal ldflags)
	AC_ARG_WITH(hal-libs,
		AS_HELP_STRING([@<:@--with-hal-libs=LDFLAGS@:>@], [linker flags for the HAL library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-hal-libs - see docs/configure.txt)
			;;
		*)
			LDFLAGS="${withval}"
			;;
		esac
	], [
		dnl also get libs from glib-2.0 to workaround a bug in dbus-glib
		LDFLAGS="`pkg-config --silence-errors --libs hal dbus-glib-1 2>/dev/null`"
		if test "$?" != "0"; then
			LDFLAGS="-lhal -ldbus-1 -lpthread"
		fi
	])
	AC_MSG_RESULT([${LDFLAGS}])

	dnl check if HAL is usable
	AC_CHECK_HEADERS(libhal.h, [nut_have_libhal=yes], [nut_have_libhal=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_HEADERS(glib.h dbus/dbus-glib.h, [], [nut_have_libhal=no], [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS(libhal_device_new_changeset, [], [nut_have_libhal=no])

	if test "${nut_have_libhal}" = "yes"; then
		AC_CHECK_FUNCS(g_timeout_add_seconds)
		LIBHAL_CFLAGS="${CFLAGS}"
		LIBHAL_LDFLAGS="${LDFLAGS}"
	fi

	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
fi
])
