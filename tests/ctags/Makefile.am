
dist_check_SCRIPTS = runner.sh

NULL =
test_sources = \
	$(NULL)
test_results = $(test_sources:=.tags)

TEST_EXTENSIONS = .tags
TAGS_LOG_COMPILER = $(srcdir)/runner.sh

TESTS = $(test_results)
EXTRA_DIST = $(test_sources) $(test_results)

runner.sh: ../../src/geany$(EXEEXT)
