
SUBDIRS = ctags

if WITH_CRITERION
LOG_COMPILER = $(srcdir)/criterion-tap-test.sh

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/build-aux/tap-driver.sh

AM_CFLAGS = $(CRITERION_CFLAGS)
LIBS += $(CRITERION_LIBS)

check_PROGRAMS = run_tests

run_tests_SOURCES = simple.c

TESTS = run_tests
endif
