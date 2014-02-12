# 
# Automake file for building liblink-grammar-java.so
#


DEFS = @DEFS@ -DVERSION=\"@VERSION@\"

# In link-grammar version 5, fat links will be gone!
if WITH_FAT_LINKS
DEFS += -DUSE_FAT_LINKAGES
endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/link-grammar $(ANSI_CFLAGS) $(LINK_CFLAGS) 

lib_LTLIBRARIES = liblink-grammar-java.la

liblink_grammar_java_la_LDFLAGS = -version-info @VERSION_INFO@ -export-dynamic -no-undefined -export-symbols $(srcdir)/link-grammar-java.def

if OS_WIN32
liblink_grammar_java_la_LDFLAGS += -D_JNI_IMPLEMENTATION_ -Wl,--kill-at
endif

liblink_grammar_java_la_LIBADD  = $(top_builddir)/link-grammar/liblink-grammar.la  ${REGEX_LIBS}

if HAVE_HUNSPELL
liblink_grammar_java_la_LIBADD  += ${HUNSPELL_LIBS}
endif

if HAVE_ASPELL
liblink_grammar_java_la_LIBADD  += ${ASPELL_LIBS}
endif

if WITH_SAT_SOLVER
liblink_grammar_java_la_LIBADD  += $(top_builddir)/link-grammar/sat-solver/libsat-solver.la
liblink_grammar_java_la_LIBADD  += $(top_builddir)/link-grammar/minisat/libminisat.la
endif

if WITH_CORPUS
liblink_grammar_java_la_LIBADD  += $(top_builddir)/link-grammar/corpus/liblink-corpus.la ${SQLITE3_LIBS}
endif

if WITH_BINRELOC
liblink_grammar_java_la_LIBADD  += -lpthread
endif

if WITH_PTHREADS
liblink_grammar_java_la_LIBADD  += -lpthread
endif

# Math libraries are needed for floorf, etc.
liblink_grammar_java_la_LIBADD  += -lm

liblink_grammar_java_la_SOURCES =   \
	jni-client.h                     \
	jni-client.c

EXTRA_DIST=                               \
	link-grammar-java.def
