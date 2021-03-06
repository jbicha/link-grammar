/*
 * Java JNI interfaces.
 *
 * This implements a very simple, low-brow, non-OOP interface.
 * It could be improved.
 */

#include <jni.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

#include <link-grammar/api-structures.h>
#include "link-grammar/corpus/corpus.h"
#include "link-grammar/error.h"
#include "jni-client.h"
#include "link-grammar/utilities.h"

/* Make the compiler shut up about the deprecated functions */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/* Default to the English language. */
static const char* in_language = "en";

typedef struct
{
	Dictionary    dict;
	Parse_Options opts, panic_parse_opts;
	Sentence      sent;
	Linkage       linkage;
	int           num_linkages, cur_linkage;
} per_thread_data;

/* XXX FIXME
 * The per_thread_data struct should ideally be somehow
 * fetched from JNIEnv, or as an opaque pointer in the class.
 * Not clear how to do this .. perhaps use NewDirectByteBuffer()
 * and the java.nio.ByteBuffer class ?
 */
#ifdef USE_PTHREADS
static pthread_key_t java_key;
static pthread_once_t java_key_once = PTHREAD_ONCE_INIT;

static void java_key_alloc(void)
{
	pthread_key_create(&java_key, free);
}
#else
static per_thread_data * global_ptd = NULL;
#endif


static per_thread_data * get_ptd(JNIEnv *env, jclass cls)
{
#ifdef USE_PTHREADS
	per_thread_data *ptd = pthread_getspecific(java_key);
#else
	per_thread_data *ptd = global_ptd;
#endif
	if (!ptd) Java_org_linkgrammar_LinkGrammar_init(env, cls);
#ifdef USE_PTHREADS
	ptd = pthread_getspecific(java_key);
#else
	ptd = global_ptd;
#endif
	return ptd;
}

static void setup_panic_parse_options(Parse_Options opts)
{
	parse_options_set_repeatable_rand(opts, false);
	parse_options_set_disjunct_cost(opts, 3.7f);
	parse_options_set_min_null_count(opts, 1);
	parse_options_set_max_null_count(opts, MAX_SENTENCE);
	parse_options_set_max_parse_time(opts, 60);
	parse_options_set_islands_ok(opts, true);
	parse_options_set_short_length(opts, 6);
	parse_options_set_all_short_connectors(opts, true);
	parse_options_set_linkage_limit(opts, 100);
	parse_options_set_verbosity(opts, 0);
	parse_options_set_spell_guess(opts, false);
}

static inline void test(void)
{
#ifdef DEBUG
	printf("%d\n", word_contains("said", PAST_TENSE_FORM_MARKER, dict));
	printf("%d\n", word_contains("gave.v", PAST_TENSE_FORM_MARKER, dict));
	printf("%d\n", word_contains("have", PAST_TENSE_FORM_MARKER, dict));
	printf("%d\n", word_contains("had", PAST_TENSE_FORM_MARKER, dict));
#endif
}

/* message: The string is encoded in modified UTF-8, per JNI 1.5 spec. */
static void throwException(JNIEnv *env, const char* message)
{
	char *msg;
	jclass exceptionClazz;
	if ((*env)->ExceptionOccurred(env) != NULL) return;

	msg = (char *) malloc(50+strlen(message));
	strcpy(msg, "link-grammar JNI:\n");
	strcat(msg, message);
	exceptionClazz = (*env)->FindClass(env, "java/lang/RuntimeException");
	if ((*env)->ThrowNew(env, exceptionClazz, msg) != 0)
		(*env)->FatalError(env, "Fatal: link-grammar JNI: Cannot throw");
}

static per_thread_data * init(JNIEnv *env, jclass cls)
{
	const char *codeset, *dict_version;
	per_thread_data *ptd;

	/* Get the locale from the environment...
	 * perhaps we should someday get it from the dictionary ??
	 */
	setlocale(LC_ALL, "");

	/* Everything breaks if the locale is not UTF-8; check for this,
	 * and force  the issue !
	 */
	codeset = nl_langinfo(CODESET);
	if (!strstr(codeset, "UTF") && !strstr(codeset, "utf"))
	{
		prt_error("Warning: JNI: locale %s was not UTF-8; force-setting to en_US.UTF-8\n",
			codeset);
		setlocale(LC_CTYPE, "en_US.UTF-8");
	}

	ptd = (per_thread_data *) malloc(sizeof(per_thread_data));
	memset(ptd, 0, sizeof(per_thread_data));

	ptd->opts = parse_options_create();

	/* Disable repeatable_rand. We'll be using Java primarily to
	 * parse large texts, and we want a good distribution when
	 * linkages overflow (very rare for mature grammars, but crucial
	 * for grammar learning. That is, for the 'any' language.
	 */
	parse_options_set_repeatable_rand(ptd->opts, false);

	/* A cost of 2.7 allows the usual cost-2 connectors, plus the
	 * assorted fractional costs, without going to cost 3.0, which
	 * is used only during panic-parsing.
	 */
	parse_options_set_disjunct_cost(ptd->opts, 2.7f);
	parse_options_set_max_parse_time(ptd->opts, 30);
	parse_options_set_linkage_limit(ptd->opts, 1000);
	parse_options_set_short_length(ptd->opts, 16);
	parse_options_set_verbosity(ptd->opts, 0);
	parse_options_set_spell_guess(ptd->opts, false);

	ptd->panic_parse_opts = parse_options_create();
	setup_panic_parse_options(ptd->panic_parse_opts);

	ptd->dict = dictionary_create_lang(in_language);
	if (!ptd->dict) throwException(env, "Error: unable to open dictionary");
	else test();

	dict_version = linkgrammar_get_dict_version(ptd->dict);
	prt_error("Info: JNI: dictionary version %s\n", dict_version);

	return ptd;
}

static void finish(per_thread_data *ptd)
{
	if (ptd->sent)
		sentence_delete(ptd->sent);
	ptd->sent = NULL;

	if (ptd->linkage)
		linkage_delete(ptd->linkage);
	ptd->linkage = NULL;

	dictionary_delete(ptd->dict);
	ptd->dict = NULL;

	parse_options_delete(ptd->opts);
	ptd->opts = NULL;

	parse_options_delete(ptd->panic_parse_opts);
	ptd->panic_parse_opts = NULL;

#ifdef USE_PTHREADS
	pthread_setspecific(java_key, NULL);
#else
	global_ptd = NULL;
#endif
	free(ptd);
}

/* ================================================================= */
/* Misc utilities */

static void jParse(JNIEnv *env, per_thread_data *ptd, char* inputString)
{
	Parse_Options opts = ptd->opts;
	int jverbosity = parse_options_get_verbosity(opts);

	/* Clean up a bit */
	if (ptd->linkage)
		linkage_delete(ptd->linkage);
	ptd->linkage = NULL;

	if (ptd->sent)
		sentence_delete(ptd->sent);

	if (ptd->dict == NULL) throwException(env, "jParse: dictionary not open\n");
	if (inputString == NULL) throwException(env, "jParse: no input sentence!\n");
	ptd->sent = sentence_create(inputString, ptd->dict);
	ptd->num_linkages = 0;

	if (ptd->sent == NULL)
		return;

	/* First parse with cost 0 or 1 and no null links */
	parse_options_set_disjunct_cost(opts, 2.7f);
	parse_options_set_min_null_count(opts, 0);
	parse_options_set_max_null_count(opts, 0);
	parse_options_reset_resources(opts);

	ptd->num_linkages = sentence_parse(ptd->sent, ptd->opts);

	/* If failed bad. Give up. */
	if (ptd->num_linkages < 0)
	{
		sentence_delete(ptd->sent);
		ptd->sent = NULL;
		return;
	}

	/* If failed, try again with null links */
	if (0 == ptd->num_linkages)
	{
		if (jverbosity > 0) prt_error("Warning: JNI: No complete linkages found.\n");
		parse_options_set_min_null_count(opts, 1);
		parse_options_set_max_null_count(opts, sentence_length(ptd->sent));
		ptd->num_linkages = sentence_parse(ptd->sent, opts);
	}

	if (jverbosity > 0)
	{
		if (parse_options_timer_expired(opts))
			prt_error("Warning: JNI: Timer is expired!\n");

		if (parse_options_memory_exhausted(opts))
			prt_error("Warning: JNI: Memory is exhausted!\n");
	}

	if ((ptd->num_linkages == 0) &&
	    parse_options_resources_exhausted(opts))
	{
		parse_options_print_total_time(opts);
		if (jverbosity > 0) prt_error("Warning: JNI: Entering \"panic\" mode...\n");
		parse_options_reset_resources(ptd->panic_parse_opts);
		parse_options_set_verbosity(ptd->panic_parse_opts, jverbosity);
		ptd->num_linkages = sentence_parse(ptd->sent, ptd->panic_parse_opts);
		if (jverbosity > 0)
		{
			if (parse_options_timer_expired(ptd->panic_parse_opts))
				prt_error("Error: JNI: Panic timer is expired!\n");
		}
	}
}

static void makeLinkage(per_thread_data *ptd)
{
	if (ptd->cur_linkage < ptd->num_linkages)
	{
		if (ptd->linkage)
			linkage_delete(ptd->linkage);

		ptd->linkage = linkage_create(ptd->cur_linkage, ptd->sent, ptd->opts);
	}
}

/* ================================================================ */
/* Java JNI wrappers */

/*
 * Class:      LinkGrammar
 * Method:     getVersion
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getVersion(JNIEnv *env, jclass cls)
{
	const char *s = linkgrammar_get_version();
	jstring j = (*env)->NewStringUTF(env, s);
	return j;
}

JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getDictVersion(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	const char *s = linkgrammar_get_dict_version(ptd->dict);
	jstring j = (*env)->NewStringUTF(env, s);
	return j;
}

JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_setMaxParseSeconds(JNIEnv *env, jclass cls, jint maxParseSeconds)
{
	per_thread_data *ptd = get_ptd(env, cls);;
	parse_options_set_max_parse_time(ptd->opts, maxParseSeconds);
}

JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_setMaxCost(JNIEnv *env, jclass cls, jdouble maxCost)
{
	per_thread_data *ptd = get_ptd(env, cls);;
	parse_options_set_disjunct_cost(ptd->opts, maxCost);
}

JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_setDictionariesPath(JNIEnv *env,
                                              jclass cls, jstring path)
{
	const char *nativePath = (*env)->GetStringUTFChars(env, path, 0);

	// Java passes null pointers as the string "null"
	if (nativePath && strcmp(nativePath, "null"))
	{
		dictionary_set_data_dir(nativePath);
	}
	(*env)->ReleaseStringUTFChars(env, path, nativePath);
}

/*
 * Class:      LinkGrammar
 * Method:     setLanguage
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_setLanguage(JNIEnv *env, jclass cls, jstring str)
{
	const char *tmp = (*env)->GetStringUTFChars(env, str, 0);
	in_language = strdup(tmp);
	(*env)->ReleaseStringUTFChars(env, str, tmp);
}

/*
 * Class:      LinkGrammar
 * Method:     setMaxLinkages
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_setMaxLinkages(JNIEnv *env, jclass cls, jint maxLinkages)
{
	per_thread_data *ptd = get_ptd(env, cls);
	parse_options_set_linkage_limit(ptd->opts, maxLinkages);
}

JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getMaxLinkages(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return parse_options_get_linkage_limit(ptd->opts);
}

/*
 * Class:      LinkGrammar
 * Method:     init
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_init(JNIEnv *env, jclass cls)
{
#ifdef USE_PTHREADS
	per_thread_data *ptd;
	pthread_once(&java_key_once, java_key_alloc);
	ptd = pthread_getspecific(java_key);
	if (ptd) return;
	ptd = init(env, cls);
	pthread_setspecific(java_key, ptd);
#else
	if (global_ptd) return;
	global_ptd = init(env, cls);
#endif
}

/*
 * Class:      LinkGrammar
 * Method:     parse
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_parse(JNIEnv *env, jclass cls, jstring str)
{
	const char *cStr;
	char * tmp;
	per_thread_data *ptd = get_ptd(env, cls);
	cStr = (*env)->GetStringUTFChars(env, str, 0);
	tmp = strdup(cStr);
	jParse(env, ptd, tmp);
	free(tmp);
	(*env)->ReleaseStringUTFChars(env, str, cStr);
}

/*
 * Class:      LinkGrammar
 * Method:     close
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_close(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	finish(ptd);
}

/*
 * Class:      LinkGrammar
 * Method:     numWords
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getNumWords(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return linkage_get_num_words(ptd->linkage);
}

JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkageWord(JNIEnv *env, jclass cls, jint i)
{
	per_thread_data *ptd = get_ptd(env, cls);

	/* Does not need to be freed, points into data structures */
	/* Returns the inflected word. */
	const char * w = linkage_get_word(ptd->linkage, i);
	jstring j = (*env)->NewStringUTF(env, w);
	return j;
}

JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkageDisjunct(JNIEnv *env, jclass cls, jint i)
{
	jstring j;
	per_thread_data *ptd = get_ptd(env, cls);

	/* does not need to be freed, points into data structures */
	/* returns the inflected word. */
	const char * w = linkage_get_disjunct_str(ptd->linkage, i);
	if (NULL == w) j = NULL;
	else j = (*env)->NewStringUTF(env, w);
	return j;
}

JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkageSense(JNIEnv *env,
                    jclass cls, jint i, jint j)
{
	per_thread_data *ptd = get_ptd(env, cls);
	Linkage lkg = ptd->linkage;
	Sense *sns;
	const char * w = NULL;
	jstring js;

	if (!lkg) return NULL;
	lg_corpus_linkage_senses(lkg);
	sns = lg_get_word_sense(lkg, i);
	while ((0 < j) && sns)
	{
		sns = lg_sense_next(sns);
		j--;
	}

	/* does not need to be freed, points into data structures */
	if (sns) w = lg_sense_get_sense(sns);

	if (w) js = (*env)->NewStringUTF(env, w);
	else js = NULL;
	return js;
}

JNIEXPORT jdouble JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkageSenseScore(JNIEnv *env,
                    jclass cls, jint i, jint j)
{
	per_thread_data *ptd = get_ptd(env, cls);
	Linkage lkg = ptd->linkage;
	Sense *sns;
	double score = 0.0;

	if (!lkg) return 0.0;
	sns = lg_get_word_sense(lkg, i);
	while ((0 < j) && sns)
	{
		sns = lg_sense_next(sns);
		j--;
	}

	if (sns) score = lg_sense_get_score(sns);

	return score;
}

/*
 * Class:      LinkGrammar
 * Method:     numSkippedWords
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getNumSkippedWords(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return sentence_null_count(ptd->sent);
}

/*
 * Class:      LinkGrammar
 * Method:     numLinkages
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getNumLinkages(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return sentence_num_valid_linkages(ptd->sent);
}

/*
 * Class:      LinkGrammar
 * Method:     makeLinkage
 * Signature: (I)I
 */
JNIEXPORT void JNICALL
Java_org_linkgrammar_LinkGrammar_makeLinkage(JNIEnv *env, jclass cls, jint i)
{
	per_thread_data *ptd = get_ptd(env, cls);
	ptd->cur_linkage = i;
	makeLinkage(ptd);
}

/*
 * Class:      LinkGrammar
 * Method:     linkageNumViolations
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkageNumViolations(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return sentence_num_violations(ptd->sent, ptd->cur_linkage);
}

/*
 * Class:      LinkGrammar
 * Method:     linkageDisjunctCost
 * Signature: ()I
 */
JNIEXPORT jdouble JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkageDisjunctCost(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return sentence_disjunct_cost(ptd->sent, ptd->cur_linkage);
}

/*
 * Class:      LinkGrammar
 * Method:     linkageLinkCost
 * Signature: ()I
 */
JNIEXPORT jdouble JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkageLinkCost(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return (jdouble) sentence_link_cost(ptd->sent, ptd->cur_linkage);
}

/*
 * Class:      LinkGrammar
 * Method:     getNumLinks
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getNumLinks(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return linkage_get_num_links(ptd->linkage);
}

/*
 * Class:      LinkGrammar
 * Method:     getLinkLWord
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkLWord(JNIEnv *env, jclass cls, jint i)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return linkage_get_link_lword(ptd->linkage, i);
}

/*
 * Class:      LinkGrammar
 * Method:     getLinkRWord
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkRWord(JNIEnv *env, jclass cls, jint i)
{
	per_thread_data *ptd = get_ptd(env, cls);
	return linkage_get_link_rword(ptd->linkage, i);
}

/*
 * Class:      LinkGrammar
 * Method:     getLinkLLabel
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkLLabel(JNIEnv *env, jclass cls, jint i)
{
	per_thread_data *ptd = get_ptd(env, cls);
	/* Does not need to be freed, points into linkage */
	const char *s = linkage_get_link_llabel(ptd->linkage, i);
	jstring j = (*env)->NewStringUTF(env, s);
	return j;
}

/*
 * Class:      LinkGrammar
 * Method:     getLinkRLabel
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkRLabel(JNIEnv *env, jclass cls, jint i)
{
	per_thread_data *ptd = get_ptd(env, cls);
	/* Does not need to be freed, points into linkage */
	const char *s = linkage_get_link_rlabel(ptd->linkage, i);
	jstring j = (*env)->NewStringUTF(env, s);
	return j;
}

/*
 * Class:      LinkGrammar
 * Method:     getLinkLabel
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkLabel(JNIEnv *env, jclass cls, jint i)
{
	per_thread_data *ptd = get_ptd(env, cls);
	/* Does not need to be freed, points into linkage */
	const char *s = linkage_get_link_label(ptd->linkage, i);
	jstring j = (*env)->NewStringUTF(env, s);
	return j;
}

/*
 * Class:      LinkGrammar
 * Method:     constituentString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getConstituentString(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	/* mode 1 prints a lisp-style string, nicely indented.
	 * mode 2 prints a lisp-style string, but with square brackets.
	 * mode 3 prints a lisp-style string, one one single line.
	 */
	/* char *s = linkage_print_constituent_tree(linkage, 1); */
	char *s = linkage_print_constituent_tree(ptd->linkage, 3);
	jstring j = (*env)->NewStringUTF(env, s);
	linkage_free_constituent_tree_str(s);
	return j;
}

/*
 * Class:      LinkGrammar
 * Method:     linkString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_linkgrammar_LinkGrammar_getLinkString(JNIEnv *env, jclass cls)
{
	per_thread_data *ptd = get_ptd(env, cls);
	char *s = linkage_print_diagram(ptd->linkage, true, 8100);
	jstring j = (*env)->NewStringUTF(env, s);
	linkage_free_diagram(s);
	return j;
}
