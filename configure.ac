AC_REVISION([$Id$])dnl
AC_INIT(libvcd/vcd.h)
dnl AM_ACLOCAL_INCLUDE(.)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(vcdimager, 0.7.14)
AM_MISSING_PROG(HELP2MAN, help2man, $missing_dir)
AM_MAINTAINER_MODE

AM_SANITY_CHECK
AC_CANONICAL_HOST

AC_DEFINE_UNQUOTED(HOST_ARCH, "$host_os/$host_cpu", [host os/cpu identifier])

dnl conditionally built frontends

AC_ARG_WITH(cli-frontend,
	[  --without-cli-frontend  enable cli frontend (enabled by default)], 
	enable_cli_fe="${withval}", enable_cli_fe=yes)

AC_ARG_WITH(xml-frontend,
	[  --without-xml-frontend  enable XML frontend (enabled by default)], 
	enable_xml_fe="${withval}", enable_xml_fe=yes)

dnl programs

AC_PROG_CC
if test "x$GCC" != "xyes" 
then
  echo "*** non GNU CC compiler detected."
  echo "*** This package has not been tested very well with non GNU compilers"
  echo "*** you should try to use 'gcc' for compiling this package."
fi

AC_PROG_CPP
AM_PROG_CC_STDC
AC_PROG_AWK
AC_PROG_RANLIB

dnl libs
AC_CHECK_FUNCS(snprintf vsnprintf, , [AC_MSG_ERROR(required function not found)])

if test "x$enable_cli_fe" = "xyes" -o "x$enable_xml_fe" = "xyes"; then
  AM_PATH_LIBPOPT(, [enable_cli_fe=no; enable_xml_fe=no])
fi

if test "x$enable_xml_fe" = "xyes"; then
  AM_PATH_XML2(2.3.8, , enable_xml_fe=no)
fi

dnl headers

AC_DEFINE(_DEVELOPMENT_, [], enable warnings about being development release)
AC_DEFINE(_GNU_SOURCE, [], enable GNU libc extension)
AC_STDC_HEADERS
AC_CHECK_HEADERS(sys/stat.h stdint.h inttypes.h stdbool.h)

if test "x$ac_cv_header_stdint_h" != "xyes" 
 then
   AC_CHECK_SIZEOF(int, 4)
   AC_CHECK_SIZEOF(long, 4)
   AC_CHECK_SIZEOF(long long, 8)
fi

dnl compiler
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE

dnl empty_array_size
AC_MSG_CHECKING([how to create empty arrays])

empty_array_size="xxx"
AC_TRY_COMPILE(,[struct { int foo; int bar[]; } doo;], empty_array_size="")

if test "x$empty_array_size" = "xxxx";then
 AC_TRY_COMPILE(,[struct { int foo; int bar[0]; } doo;], empty_array_size="0")
fi

if test "x$empty_array_size" = "xxxx"
then
  AC_MSG_ERROR([compiler is unable to creaty empty arrays])
else
  AC_DEFINE_UNQUOTED(EMPTY_ARRAY_SIZE, $empty_array_size, 
	[what to put between the brackets for empty arrays])
  changequote(`,')
  msg="[${empty_array_size}]"
  changequote([,])
  AC_MSG_RESULT($msg)
fi
dnl empty_array_size

dnl bitfield order
AC_MSG_CHECKING(bitfield ordering in structs)
AC_TRY_RUN([
int
 main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
  bf = { 1,1,1,1 };
  if (sizeof (bf) != 1) return 1;
  return *((unsigned char*) &bf) != 0x4b; }
], bf_lsbf=1, AC_TRY_RUN([ 
int
main() {
  struct { char bit_0:1, bit_12:2, bit_345:3, bit_67:2; }
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
  __attribute__((packed))
#endif
  bf = { 1,1,1,1 };
  if (sizeof (bf) != 1) return 1;
  return *((unsigned char*) &bf) != 0xa5; }
], bf_lsbf=0, AC_MSG_ERROR([unsupported bitfield ordering])))
if test "x$bf_lsbf" = "x1"; then
 AC_MSG_RESULT(LSBF)
 AC_DEFINE(BITFIELD_LSBF, [], [compiler does lsbf in struct bitfields])
else
 AC_MSG_RESULT(MSBF)
fi
dnl

dnl system
AC_CYGWIN

if test "x$GCC" = "xyes"
then
  WARN_CFLAGS="-Wall -Wchar-subscripts -Wmissing-prototypes -Wmissing-declarations -Wunused -Wpointer-arith -Wwrite-strings -Wnested-externs"
fi

LIBS="$LIBS -lm"
CFLAGS="$CFLAGS $WARN_CFLAGS"

AM_CONDITIONAL(CYGWIN, test "x$CYGWIN" = "xyes")
AM_CONDITIONAL(BUILD_CLI_FE, test "x$enable_cli_fe" = "xyes")
AM_CONDITIONAL(BUILD_XML_FE, test "x$enable_xml_fe" = "xyes")

LIBVCD_CFLAGS='-I$(top_srcdir)/libvcd/'
LIBVCD_LIBS='$(top_builddir)/libvcd/libvcd.a'
LIBVCDINFO_LIBS='$(top_builddir)/libvcd/libvcdinfo.a'
AC_SUBST(LIBVCD_CFLAGS)
AC_SUBST(LIBVCD_LIBS)
AC_SUBST(LIBVCDINFO_LIBS)

case $host in
     *-linux*)
        AC_CHECK_HEADERS(linux/version.h)
	LIBVCD_SYSDEP='vcd_image_cd-linux.$(OBJEXT)' 
	;;
     *-bsdi*)
	LIBVCD_SYSDEP='vcd_image_cd-bsdi.$(OBJEXT)' 
	;;
     *-sunos*|*-sun-*)
        AC_CHECK_HEADERS(sys/cdio.h)
	LIBVCD_SYSDEP='vcd_image_cd-sunos.$(OBJEXT)' 
	;;
     *)
	LIBVCD_SYSDEP='vcd_image_cd-generic.$(OBJEXT)' 
	;;
esac
AC_SUBST(LIBVCD_SYSDEP)

AC_OUTPUT([ \
	Makefile \
	amiga/Makefile \
	libvcd/Makefile \
	frontends/Makefile \
	frontends/xml/Makefile \
	frontends/cli/Makefile \
	docs/Makefile \
	tests/Makefile \
	vcdimager.spec \
])

echo "
Setup:
  Install path:     ${prefix}
  Build CLI FE:	    $enable_cli_fe
  Build XML FE:	    $enable_xml_fe
  Maintainer mode:  $enable_maintainer_mode
"

if test "x$enable_cli_fe" != "xyes" -a "x$enable_xml_fe" != "xyes"
then
  cat <<EOF
****************************************************************************
****************************************************************************
***                                                                      ***
***         WARNING: BUILDING ONLY LIBVCD W/O ANY FRONTENDS              ***
***      (maybe you are missing some library (libpopt, libxml2)?         ***
***                                                                      ***
****************************************************************************
****************************************************************************
EOF
fi

dnl EOF
