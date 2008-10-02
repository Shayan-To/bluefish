/* for the design docs see bftextview2.h */
#include <string.h>
#include "bftextview2_patcompile.h"

/*
we need real regex pattern support as well. 

we don't do everything that pcre or posix regex patterns can 
do - these engines have features that cannot be done in a DFA  

There are several ways in which regex patterns can be simplified: 

a(bc)+ == abc(abc)*
[a-z]+ == [a-z][a-z]*
OR reverse:
a(bc)* == (a|a[bc]+)
a[a-z]* == (a|a[a-z]+)

a(bc)? = (a|abc)
a[a-z]? = (a|a[a-z])

a(b|c)d == (abd|acd)

[^a] == [b-z] (and all other ascii characters, for simplification I just use the alphabet)

so we need to be able to compile:
the OR construct: (|)
the zero-or-more *
the character list [a-z]

*/
static void print_characters(gchar *characters) {
	int i;
	DBG_PATCOMPILE("we have active characters ");
	for (i=0;i<NUMSCANCHARS;i++) {
		if (characters[i]==1)
			DBG_PATCOMPILE("%c ",i);
	}
	DBG_PATCOMPILE("\n");
}

static gint fill_characters_from_range(gchar *input, gchar *characters) {
	gboolean reverse = 0;
	gint i=0;
	if (input[i] == '^') {	/* see if it is a NOT pattern */
		reverse = 1;
		memset(characters, 1, NUMSCANCHARS*sizeof(char));
		i++;
	}
	if (input[i] == ']') {	/* ] is actually part of the collection */
		characters['['] = 1;
		i++;
	}
	if (input[i] == '-') {	/* - is actually part of the collection */
		characters['-'] = 1;
		i++;
	}
	while (input[i] != ']') {
		if (input[i] == '-') {	/* range all characters between the previous and the next char */
			gchar j;
			DBG_PATCOMPILE("set characters from %c to %c to %d\n",input[i - 1],input[i+1],1-reverse);
			for (j = input[i - 1]; j <= input[i + 1]; j++) {
				characters[(int)j] = 1 - reverse;
			}
			i += 2;
		} else {
			DBG_PATCOMPILE("set characters %c (pos %d) to %d\n",input[i],input[i],1-reverse);
			characters[(int)input[i]] = 1 - reverse;
			i++;
		}
	}
	return i;
}

static void create_state_tables(Tscantable *st, guint context, gchar *characters, gboolean pointtoself, GQueue *positions, GQueue *newpositions) {
	guint c,pos;
	guint identstate;
	gint newstate=-1; /* if all characters can follow existing states we don't need any newstate
	and thus newstate==-1 but if one or more characters in one or more states need a new state
	it will be >0 */
	identstate = g_array_index(st->contexts, Tcontext, context).identstate;
	DBG_PATCOMPILE("started for %d states, pointtoself=%d\n",g_queue_get_length(positions),pointtoself);
	while (g_queue_get_length(positions)) {
		pos = GPOINTER_TO_INT(g_queue_pop_head(positions));
		DBG_PATCOMPILE("working on position %d, identstate=%d\n",pos,identstate);
		for (c=0;c<NUMSCANCHARS;c++) {
			if (characters[c] == 1) {
				/*DBG_PATCOMPILE("running for position %d char %c\n",pos,c);*/
				if (g_array_index(st->table, Ttablerow, pos).row[c] != 0 && g_array_index(st->table, Ttablerow, pos).row[c] != identstate) {
					if (pointtoself) { /* perhaps check here if the state does point to itself, 
							or if we have this state on the stack already */
						g_queue_push_head(positions, GINT_TO_POINTER((gint)g_array_index(st->table, Ttablerow, pos).row[c]));
					} else {
						g_queue_push_head(newpositions, GINT_TO_POINTER((gint)g_array_index(st->table, Ttablerow, pos).row[c]));
					}
				} else {
					if (newstate == -1) {
						g_array_index(st->table, Ttablerow, pos).row[c] = newstate = st->table->len;
						DBG_PATCOMPILE("create newstate %d, pointtoself=%d\n",newstate,pointtoself);
						g_array_set_size(st->table,st->table->len+1);
						memcpy(g_array_index(st->table, Ttablerow, newstate).row, g_array_index(st->table, Ttablerow, g_array_index(st->contexts, Tcontext, context).identstate).row, sizeof(unsigned int[NUMSCANCHARS]));
						g_queue_push_head(newpositions, GINT_TO_POINTER(newstate));
						if (pointtoself) {
							guint d;
							for (d=0;d<NUMSCANCHARS;d++) {
								if (characters[d]==1) { 
									/*DBG_PATCOMPILE("in newstate %d, character %c points to %d\n",newstate,d,newstate);*/
									g_array_index(st->table, Ttablerow, newstate).row[d] = newstate;
								}
							}
						}
					} else {
						g_array_index(st->table, Ttablerow, pos).row[c] = newstate;
						/* nothing to put on the stack, newstate should be on newpositions already if it is not -1 */
					}
				}
			}
		}
	}
}

static void merge_queues(GQueue *target, GQueue *src) {
	while (g_queue_get_length(src)) {
		DBG_PATCOMPILE("merge queue, push state %d to queue\n",GPOINTER_TO_INT(g_queue_peek_head(src)));
		g_queue_push_head(target,g_queue_pop_head(src)); 
	}
	DBG_PATCOMPILE("merge queue, target queue has length %d \n",g_queue_get_length(target));
}

static GQueue *process_regex_part(Tscantable *st, gchar *regexpart,guint context, gboolean caseinsensitive, GQueue *inputpositions);

static GQueue *run_subpatterns(Tscantable *st, gchar *regexpart,guint context, gboolean caseinsensitive, GQueue *inputpositions, gint *regexpartpos) {
	gint j=0;
	gboolean escaped=FALSE;
	gchar *target;
	GQueue *mergednewpositions = g_queue_new();
	target = g_strdup(&regexpart[*regexpartpos]); /* a very easy way to make target a buffer long enough to hold any subpattern */
	
	while (!escaped && regexpart[*regexpartpos] != '\0') {
		DBG_PATCOMPILE("run_subpatterns, regexpart[%d]=%c\n",*regexpartpos,regexpart[*regexpartpos]);
		if (!escaped && regexpart[*regexpartpos] == '\\') {
			escaped = TRUE;
			*regexpartpos = *regexpartpos + 1;
		}
		if (!escaped && (regexpart[*regexpartpos] == '|' || regexpart[*regexpartpos] == ')')) {
			/* found a subpattern */
			GQueue *newpositions;
			target[j] = '\0';
			DBG_PATCOMPILE("at regexpartpos=%d found SUBPATTERN %s\n",*regexpartpos,target);
			newpositions = process_regex_part(st, target,context, caseinsensitive, inputpositions);
			merge_queues(mergednewpositions,newpositions);
			g_queue_free(newpositions);
			j=0;
			
			if (regexpart[*regexpartpos] == ')') {
				g_free(target);
				return mergednewpositions;
			}
		} else {
			target[j] = regexpart[*regexpartpos];
			j++;
		}
		*regexpartpos = *regexpartpos + 1;
		escaped=FALSE;
	}
	g_free(target);
	return mergednewpositions;
}

static GQueue *process_regex_part(Tscantable *st, gchar *regexpart,guint context, gboolean caseinsensitive, GQueue *inputpositions) {
	gboolean escaped = FALSE;
	gint i=0;
	gchar characters[NUMSCANCHARS];
	GQueue *newpositions, *positions, *tmp;
	
	positions = g_queue_copy(inputpositions);
	newpositions = g_queue_new();	
	
	while (1) {
		memset(&characters, 0, NUMSCANCHARS*sizeof(char));
		escaped = 0;
		DBG_PATCOMPILE("start of loop, regexpart[%d]=%c, have %d positions\n",i,regexpart[i],g_queue_get_length(positions));
		if (regexpart[i] == '\0') { /* end of pattern */
			DBG_PATCOMPILE("end of pattern, positions(%p) has %d entries\n",positions,g_queue_get_length(positions));
			/* BUG: ?? check if the last character was a symbol ???? */
			
			
			g_queue_free(newpositions);
			return positions;
		} else {
			if (!escaped && regexpart[i]=='\\') {
				escaped = TRUE;
				i++;
			}
			if (!escaped && regexpart[i] == '(') {
				/* a subpattern */
				DBG_PATCOMPILE("found subpatern start at %d\n",i);
				i++;
				newpositions = run_subpatterns(st, regexpart,context, caseinsensitive, positions, &i);
				DBG_PATCOMPILE("end of subpatern at %d (%c)\n",i,regexpart[i]);
			} else { 
				if (!escaped) {
					switch (regexpart[i]) {
						case '.':
							memset(&characters, 1, NUMSCANCHARS*sizeof(char));
						break;
						case '[':
							DBG_PATCOMPILE("found range at i=%d, fill characters\n",i);
							i += fill_characters_from_range(&regexpart[i+1],characters) + 1;
						break;
						default:
							characters[(int)regexpart[i]] = 1;
						break;
					}
				} else { /* escaped */
					characters[(int)regexpart[i]] = 1;
				}
				/* handle case */
				if (caseinsensitive) {
					gint j;
					for (j = 'a'; j <= 'z'; j++) {
						characters[j - 32] = characters[j];
					}
				}
				DBG_PATCOMPILE("i=%d, testing i+1  (%c) for operator\n",i,regexpart[i+1]);
				/*print_characters(characters);*/
				/* see if there is an operator */
				if (regexpart[i] != '\0' && regexpart[i+1] == '+') {
					create_state_tables(st, context, characters, TRUE, positions, newpositions);
					i++;
				} else {
					create_state_tables(st, context, characters, FALSE, positions, newpositions);
				}
			}
			g_queue_clear(positions);
			tmp = positions;
			positions = newpositions;
			newpositions = tmp;
		}
		i++;
	}
}

static void compile_limitedregex_to_DFA(Tscantable *st, gchar *input, gboolean caseinsensitive, guint matchnum, guint context) {
	GQueue *positions, *newpositions;
	gchar *lregex;
	gint p;
	positions = g_queue_new();
	
	if (caseinsensitive) {
		/* make complete string lowercase */
		lregex = g_ascii_strdown(input,-1);
	} else {
		lregex = g_strdup(input);
	}
	
	g_queue_push_head(positions, GINT_TO_POINTER((gint)g_array_index(st->contexts, Tcontext, context).startstate));
	DBG_PATCOMPILE("lregex=%s, positionstack has length %d\n",lregex,g_queue_get_length(positions));
	newpositions = process_regex_part(st, lregex,context, caseinsensitive, positions);
	/*compile_limitedregex_to_DFA_backend(st,lregex,context,caseinsensitive,&positions);*/
	DBG_PATCOMPILE("after compiling positionstack has length %d\n",g_queue_get_length(positions));
	while ((g_queue_get_length(newpositions))) {
		p = GPOINTER_TO_INT(g_queue_pop_head(newpositions));
		DBG_PATCOMPILE("mark state %d as possible end-state\n",p);
		g_array_index(st->table, Ttablerow, p).match = matchnum;
	}
	g_queue_free(positions);
	g_queue_free(newpositions);
	g_free(lregex);
}

guint16 new_context(Tscantable *st, gchar *symbols, GtkTextTag *contexttag) {
	guint context, startstate, identstate;
	gint i;
	gchar *tmp;
	
	context = st->contexts->len;
	g_array_set_size(st->contexts,st->contexts->len+1);

	startstate = st->table->len;
	identstate = st->table->len+1;

	g_array_index(st->contexts, Tcontext, context).startstate = startstate;
	g_array_index(st->contexts, Tcontext, context).identstate = identstate;
	g_array_index(st->contexts, Tcontext, context).contexttag = contexttag;
	g_array_set_size(st->table,st->table->len+2);

	DBG_PATCOMPILE("new context %d has startstate %d, identstate %d and symbols %s\n",context, g_array_index(st->contexts, Tcontext, context).startstate, g_array_index(st->contexts, Tcontext, context).identstate,symbols);	
	/* identstate refers to itself for all characters except the symbols. we cannot use memset
	because an integer occupies 2 bytes */
	for (i=0;i<NUMSCANCHARS;i++)
		g_array_index(st->table, Ttablerow, identstate).row[i] = identstate;
	
	tmp = symbols;
	while (*tmp) {
		/*g_print("mark %c as symbol\n",*tmp);*/
		g_array_index(st->table, Ttablerow, identstate).row[(int)*tmp] = 0;
		tmp++;
	} 
	memcpy(g_array_index(st->table, Ttablerow, startstate).row, g_array_index(st->table, Ttablerow, identstate).row, sizeof(guint16[NUMSCANCHARS]));
	
		
	return context;
}

void match_set_nextcontext(Tscantable *st, guint16 matchnum, guint16 nextcontext) {
	g_print("set match %d to have nextcontext %d\n",matchnum,nextcontext);
	g_array_index(st->matches, Tpattern, matchnum).nextcontext = nextcontext;
}

static guint16 new_match(Tscantable *st, gchar *keyword, GtkTextTag *selftag, guint context, guint nextcontext
				, gboolean starts_block, gboolean ends_block, guint blockstartpattern
				, GtkTextTag *blocktag,gboolean add_to_ac, gchar *reference) {
	guint matchnum;
/* add the match */
	
	matchnum = st->matches->len;
	DBG_PATCOMPILE("new match %s with matchnum %d\n",keyword,matchnum);
	g_array_set_size(st->matches,st->matches->len+1);

	g_array_index(st->matches, Tpattern, matchnum).message = g_strdup(keyword);
	g_array_index(st->matches, Tpattern, matchnum).ends_block = ends_block;
	g_array_index(st->matches, Tpattern, matchnum).starts_block = starts_block;
	g_array_index(st->matches, Tpattern, matchnum).blockstartpattern = blockstartpattern;
	g_array_index(st->matches, Tpattern, matchnum).selftag = selftag;
	g_array_index(st->matches, Tpattern, matchnum).blocktag = blocktag;
	g_array_index(st->matches, Tpattern, matchnum).nextcontext = nextcontext;
	
		/* do autocompletion and reference */
	if (add_to_ac) {
		GList *list;
		if (!g_array_index(st->contexts, Tcontext, context).ac) {
			DBG_PATCOMPILE("create g_completion for context %d\n",context);
			g_array_index(st->contexts, Tcontext, context).ac = g_completion_new(NULL);
		}
		list = g_list_prepend(NULL, g_strdup(keyword));
		g_completion_add_items(g_array_index(st->contexts, Tcontext, context).ac, list);
		DBG_AUTOCOMP("adding %s to GCompletion\n",(gchar *)list->data);
		g_list_free(list);
		
		if (reference) {
			if (!g_array_index(st->contexts, Tcontext, context).reference) {
				DBG_PATCOMPILE("create hashtable for context %d\n",context);
				g_array_index(st->contexts, Tcontext, context).reference = g_hash_table_new(g_str_hash,g_str_equal);
			}
			g_hash_table_insert(g_array_index(st->contexts, Tcontext, context).reference,keyword,reference);
		}
	}
	
	return matchnum;
}

#define character_is_symbol(st,context,c) (g_array_index(st->table, Ttablerow, context.identstate).row[c] != context.identstate)

/* this function cannot do any regex style patterns 
just full keywords */
static void compile_keyword_to_DFA(Tscantable *st, gchar *keyword, guint16 matchnum, guint16 context) {
	gint i,len,pos=0;
	guint16 identstate = g_array_index(st->contexts, Tcontext, context).identstate;

	/* compile the keyword into the DFA */
	pos = g_array_index(st->contexts, Tcontext, context).startstate;
/*	if (strlen(keyword)==1 && g_array_index(st->table, Ttablerow, identstate).row[(int)keyword[0]] == 0) {
		/ * this keyword is a symbol itself ! * /
		is_symbol=TRUE;
	}*/
	
	DBG_PATCOMPILE("in context %d we start with position %d; %d is the identstate\n",context,pos,identstate);
	len = strlen(keyword);
	for (i=0;i<=len;i++) {
		int c = keyword[i];
		if (c == '\0') {
			if (character_is_symbol(st,g_array_index(st->contexts, Tcontext, context),(gint)keyword[i-1])) {
				for (i=0;i<NUMSCANCHARS;i++)
					if (g_array_index(st->table, Ttablerow, pos).row[i] == identstate)
						g_array_index(st->table, Ttablerow, pos).row[i] = 0;
			}
			g_array_index(st->table, Ttablerow, pos).match = matchnum;
		} else {
			DBG_PATCOMPILE("state %d, character %c (before change)) refers to next state %d\n",pos,c,g_array_index(st->table, Ttablerow, pos).row[c]);
			if (g_array_index(st->table, Ttablerow, pos).row[c] != 0 && g_array_index(st->table, Ttablerow, pos).row[c] != identstate) {
				pos = g_array_index(st->table, Ttablerow, pos).row[c];
			} else {
				pos = g_array_index(st->table, Ttablerow, pos).row[c] = st->table->len;
				g_array_set_size(st->table,st->table->len+1);
				memcpy(g_array_index(st->table, Ttablerow, pos).row, g_array_index(st->table, Ttablerow, g_array_index(st->contexts, Tcontext, context).identstate).row, sizeof(guint16[NUMSCANCHARS]));
			}
		}
	}
}

guint16 add_keyword_to_scanning_table(Tscantable *st, gchar *keyword, gboolean is_regex,gboolean case_insens, GtkTextTag *selftag, guint context, guint nextcontext
				, gboolean starts_block, gboolean ends_block, guint blockstartpattern
				, GtkTextTag *blocktag,gboolean add_to_ac, gchar *reference)  {
	guint16 matchnum;
	
	matchnum = new_match(st, keyword, selftag, context, nextcontext, starts_block, ends_block, blockstartpattern
				, blocktag,add_to_ac, reference);
	DBG_PATCOMPILE("add_keyword_to_scanning_table,keyword=%s,starts_block=%d,ends_block=%d,blockstartpattern=%d, context=%d,nextcontext=%d and got matchnum %d\n",keyword, starts_block, ends_block, blockstartpattern,context,nextcontext,matchnum);
	if (is_regex) {
		compile_limitedregex_to_DFA(st, keyword, case_insens, matchnum, context);
	} else {
		compile_keyword_to_DFA(st, keyword, matchnum, context);
	}
	return matchnum;
}

void print_DFA(Tscantable *st, char start, char end) {
	gint i,j;
	g_print("    ");
	for (j=start;j<=end;j++) {
		g_print(" %c ",j);
	}
	g_print(": match\n");
	for (i=0;i<st->table->len;i++) {
		g_print("%2d: ",i);
		for (j=start;j<=end;j++) {
			g_print("%2d ",g_array_index(st->table, Ttablerow, i).row[j]);
		}
		g_print(": %2d\n",g_array_index(st->table, Ttablerow, i).match);
	}
	
}

Tscantable *scantable_new() {
	Tscantable *st;
	st = g_slice_new0(Tscantable);
	st->table = g_array_sized_new(TRUE,TRUE,sizeof(Ttablerow), 160);
	st->contexts = g_array_sized_new(TRUE,TRUE,sizeof(Tcontext), 3);
	st->matches = g_array_sized_new(TRUE,TRUE,sizeof(Tpattern), 10);
	st->matches->len = 1; /* match 0 eans no match */
	return st;
}

void print_scantable_stats(Tscantable *st) {
	g_print("%d states (%.2f Kbytes)\n",st->table->len,1.0*st->table->len*sizeof(Ttablerow)/1024.0);
	g_print("%d contexts (%.2f Kbytes)\n",st->contexts->len,1.0*st->contexts->len*sizeof(Tcontext)/1024.0);
	g_print("%d matches (%.2f Kbytes)\n",st->matches->len,1.0*st->matches->len*sizeof(Tpattern)/1024.0);

}
#ifdef HARDCODED_PATTERNS
static void add_html_tag(Tscantable *st, guint context, GtkTextTag *tag, GtkTextTag *attrib, GtkTextTag *string, gchar *tagname, gchar *attribname, ...) {
	guint contexttag, contextstring, match;
	gchar *tmp;
	va_list args;
	
	contexttag = new_context(st, ">\"=' \t\n\r", NULL);
	tmp = g_strconcat("<",tagname,NULL);
	match = add_keyword_to_scanning_table(st, tmp, FALSE, FALSE, tag, context, contexttag, TRUE, FALSE, 0, NULL,TRUE,NULL);
	g_free(tmp);
	add_keyword_to_scanning_table(st, ">", FALSE, FALSE, tag, contexttag, context, FALSE, FALSE, 0, NULL,FALSE,NULL);

	va_start(args, attribname);
	while (attribname) {
		add_keyword_to_scanning_table(st, attribname, FALSE, FALSE, attrib, contexttag, contexttag, FALSE, FALSE, 0, NULL,TRUE,NULL);
		attribname = va_arg(args, gchar*);
	}
	va_end(args);
	
	contextstring = new_context(st, "\"=' \t\n\r", string);
	add_keyword_to_scanning_table(st, "\"", FALSE, FALSE, string, contexttag, contextstring, FALSE, FALSE, 0, NULL,FALSE,NULL);
	add_keyword_to_scanning_table(st, "\"", FALSE, FALSE, string, contextstring, contexttag, FALSE, FALSE, 0, string,FALSE,NULL);

	tmp = g_strconcat("</",tagname,">",NULL);
	add_keyword_to_scanning_table(st, tmp, FALSE, FALSE, tag, context, context, FALSE, TRUE, match, NULL,TRUE,NULL);
	g_free(tmp);

}

Tscantable *bftextview2_scantable_new(GtkTextBuffer *buffer) {
	Tscantable *st;
	gint i,context1,context2,match1;
	GtkTextTag *braces, *comment, *storage, *keyword, *string, *variable, *function, *value, *tag,*region;
	
	braces = gtk_text_buffer_create_tag(buffer,"braces","weight", PANGO_WEIGHT_BOLD,"foreground","darkblue",NULL);
	comment = gtk_text_buffer_create_tag(buffer,"comment","style", PANGO_STYLE_ITALIC,"foreground", "grey", NULL);
	storage = gtk_text_buffer_create_tag(buffer,"storage","weight", PANGO_WEIGHT_BOLD,"foreground", "darkred", NULL);
	keyword = gtk_text_buffer_create_tag(buffer,"keyword","weight", PANGO_WEIGHT_BOLD,"foreground", "black", NULL);
	string = gtk_text_buffer_create_tag(buffer,"string","foreground", "#008800", NULL);
	variable = gtk_text_buffer_create_tag(buffer,"variable","foreground", "red", "weight", PANGO_WEIGHT_BOLD , NULL);
	value = gtk_text_buffer_create_tag(buffer,"value","foreground", "blue", NULL);
	function = gtk_text_buffer_create_tag(buffer,"function","foreground", "darkblue", NULL);
	tag = gtk_text_buffer_create_tag(buffer,"tag","foreground", "#880088", NULL);
	region = gtk_text_buffer_create_tag(buffer,"region","background", "#EEF8FF", NULL);

	st = scantable_new();

/*#define REGEXCOMPILING*/
#ifdef REGEXCOMPILING
	context1 = new_context(st," \t\n;(){}[]:\"\\',<>*&^%!+=-|/?#");
	match1 = new_match(st, "numbers", variable, context1, context1, FALSE, FALSE, 0, NULL,FALSE, NULL);
	compile_limitedregex_to_DFA(st, "[0-9.]+", FALSE, match1, context1);
	match1 = new_match(st, "comment", comment, context1, context1, FALSE, FALSE, 0, NULL,FALSE, NULL);
	compile_limitedregex_to_DFA(st, "(#|//)[^\n]+", FALSE, match1, context1);
	match1 = new_match(st, "storage", storage, context1, context1, FALSE, FALSE, 0, NULL,FALSE, NULL);
	compile_limitedregex_to_DFA(st, "(void|int|char)", FALSE, match1, context1);
/*	add_keyword_to_scanning_table(st, "void", storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "A function without return value returns <b>void</b>. An argument list for a function taking no arguments is also <b>void</b>. The only variable that can be declared with type void is a pointer.");
	add_keyword_to_scanning_table(st, "int", storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "Integer bla bla");
	add_keyword_to_scanning_table(st, "char", storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");*/
	print_DFA(st, '#', '/');	
#endif

/* #define C_PATERNS */
#ifdef C_PATERNS
	context1 = new_context(st," \t\n;(){}[]:\"\\',<>*&^%!+=-|/?#");
	add_keyword_to_scanning_table(st, "void", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "A function without return value returns <b>void</b>. An argument list for a function taking no arguments is also <b>void</b>. The only variable that can be declared with type void is a pointer.");
	add_keyword_to_scanning_table(st, "int", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "Integer bla bla");
	add_keyword_to_scanning_table(st, "char", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "gchar", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "gint", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "guint", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "gpointer", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "gboolean", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GList", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GFile", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkWidget", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkTextView", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkTextBuffer", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkLabel", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkTextIter", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkTextMark", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkTreeStore", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkListStore", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GtkTreeModel", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	add_keyword_to_scanning_table(st, "GObject", FALSE, FALSE, storage, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "storage type bla bla");
	
	add_keyword_to_scanning_table(st, "NULL", FALSE, FALSE, variable, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "empty pointer value bla bla");
	add_keyword_to_scanning_table(st, "TRUE", FALSE, FALSE, variable, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "empty pointer value bla bla");
	add_keyword_to_scanning_table(st, "FALSE", FALSE, FALSE, variable, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "empty pointer value bla bla");
	
	add_keyword_to_scanning_table(st, "if", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "if bla");
	add_keyword_to_scanning_table(st, "else", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "else bla bla");
	add_keyword_to_scanning_table(st, "return", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "return bla bla");
	add_keyword_to_scanning_table(st, "static", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "static bla bla");
	add_keyword_to_scanning_table(st, "const", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "const bla bla");
	add_keyword_to_scanning_table(st, "sizeof", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "sizeof bla bla");
	add_keyword_to_scanning_table(st, "switch", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "switch bla bla");
	add_keyword_to_scanning_table(st, "case", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "case bla bla");
	add_keyword_to_scanning_table(st, "break", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "break bla bla");
	add_keyword_to_scanning_table(st, "typedef", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "break bla bla");
	add_keyword_to_scanning_table(st, "struct", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "break bla bla");
	add_keyword_to_scanning_table(st, "enum", FALSE, FALSE, keyword, context1, context1, FALSE, FALSE, 0, NULL,TRUE, "break bla bla");
	match1 = add_keyword_to_scanning_table(st, "{", FALSE, FALSE, braces, context1, context1, TRUE, FALSE, 0, NULL,FALSE,NULL);
	add_keyword_to_scanning_table(st, "}", FALSE, FALSE, braces, context1, context1, FALSE, TRUE, match1, NULL,FALSE,NULL);
	match1 = add_keyword_to_scanning_table(st, "(", FALSE, FALSE, braces, context1, context1, TRUE, FALSE, 0, NULL,FALSE,NULL);
	add_keyword_to_scanning_table(st, ")", FALSE, FALSE, braces, context1, context1, FALSE, TRUE, match1, NULL,FALSE,NULL);
	match1 = add_keyword_to_scanning_table(st, "[", FALSE, FALSE, braces, context1, context1, TRUE, FALSE, 0, NULL,FALSE,NULL);
	add_keyword_to_scanning_table(st, "]", FALSE, FALSE, braces, context1, context1, FALSE, TRUE, match1, NULL,FALSE,NULL);
	context2 = new_context(st,"\\\" \n\t");
	match1 = add_keyword_to_scanning_table(st, "\"", FALSE, FALSE, string, context1, context2, TRUE, FALSE, 0, NULL,FALSE,NULL);
	add_keyword_to_scanning_table(st, "\\\"", FALSE, FALSE, string, context2, context2, FALSE, FALSE, 0, string,FALSE,NULL);
	add_keyword_to_scanning_table(st, "\"", FALSE, FALSE, string, context2, context1, FALSE, TRUE, match1, string,FALSE,NULL);
	context2 = new_context(st, "*/ \n\t");
	match1 = add_keyword_to_scanning_table(st, "/*", FALSE, FALSE, comment, context1, context2, TRUE, FALSE, 0, NULL,FALSE,NULL);
	add_keyword_to_scanning_table(st, "*/", FALSE, FALSE, comment, context2, context1, FALSE, TRUE, match1, comment,FALSE,NULL);
	add_keyword_to_scanning_table(st, "[0-9.]+", TRUE, FALSE, variable, context1, context1, FALSE, FALSE, 0, NULL,FALSE,NULL);
	add_keyword_to_scanning_table(st, "//[^\n\r]+", TRUE, FALSE, comment, context1, context1, FALSE, FALSE, 0, NULL,FALSE,NULL);
#endif /* C_PATERNS */

#define PHP_COMPILING
#ifdef PHP_COMPILING
	{
		guint context0, contexttag, contextphp, contextstring;
		guint match;
		context0= new_context(st, "<> \n\t\r",NULL);

		add_html_tag(st, context0, tag, keyword, string, "html", NULL);
		add_html_tag(st, context0, tag, keyword, string, "head", NULL);
		add_html_tag(st, context0, tag, keyword, string, "link", "rel","type",NULL);
		add_html_tag(st, context0, tag, keyword, string, "title", NULL);
		add_html_tag(st, context0, tag, keyword, string, "body", "onLoad", "style", "class",NULL);
		add_html_tag(st, context0, tag, keyword, string, "p", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "div", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "span", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "ul", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "li", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "h1", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "h2", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "h3", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "h4", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "h5", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "h6", "style", "class", "id", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "meta", "name", "content", "http-equiv", "align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "b", "style", "class", "id",NULL);
		add_html_tag(st, context0, tag, keyword, string, "i", "style", "class", "id",NULL);
		add_html_tag(st, context0, tag, keyword, string, "em", "style", "class", "id",NULL);
		add_html_tag(st, context0, tag, keyword, string, "strong", "style", "class", "id",NULL);
		add_html_tag(st, context0, tag, keyword, string, "img", "style", "class", "id","src","alt","width","height","border","valign","align",NULL);
		add_html_tag(st, context0, tag, keyword, string, "script", "type", "src", NULL);
		add_html_tag(st, context0, tag, keyword, string, "a", "style", "class", "id", "href", "target","title",NULL);
		

		contexttag = new_context(st, "-> \t\n\r",NULL);
		match = add_keyword_to_scanning_table(st, "<!--", FALSE, FALSE, comment, context0, contexttag, TRUE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "-->", FALSE, FALSE, comment, contexttag, context0, FALSE, TRUE, match, comment,FALSE,NULL);

		contextphp = new_context(st, "\"=' \t\n\r$(){}[]*+-/\\,;<>?:.",region);
		match = add_keyword_to_scanning_table(st, "<?php", FALSE, FALSE, variable, context0, contextphp, TRUE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "?>", FALSE, FALSE, variable, contextphp, context0, FALSE, TRUE, match, NULL,FALSE,NULL);

		add_keyword_to_scanning_table(st, "if", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "return", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "function", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "else", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "case", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "switch", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "break", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "echo", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "class", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "var", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "while", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "for", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);

		add_keyword_to_scanning_table(st, "FALSE", FALSE, FALSE, value, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "TRUE", FALSE, FALSE, value, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);	
	
		contextstring = new_context(st, "\\\"",string);
		add_keyword_to_scanning_table(st, "\"", FALSE, FALSE, string, contextphp, contextstring, FALSE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, "\"", FALSE, FALSE, string, contextstring, contextphp, FALSE, FALSE, 0, string,FALSE,NULL);
		add_keyword_to_scanning_table(st, "\\\"", FALSE, FALSE, string, contextstring, contextstring, FALSE, FALSE, 0, NULL,FALSE,NULL);

		contextstring = new_context(st, "\"=' \t\n\r",NULL);
		match = add_keyword_to_scanning_table(st, "/*", FALSE, FALSE, comment, contextphp, contextstring, TRUE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, "*/", FALSE, FALSE, comment, contextstring, contextphp, FALSE, TRUE, match, comment,FALSE,NULL);

		contextstring = new_context(st, "\"=' \t\n\r",string);
		add_keyword_to_scanning_table(st, "'", FALSE, FALSE, string, contextphp, contextstring, FALSE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, "'", FALSE, FALSE, string, contextstring, contextphp, FALSE, FALSE, 0, string,FALSE,NULL);
		add_keyword_to_scanning_table(st, "\\'", FALSE, FALSE, string, contextstring, contextstring, FALSE, FALSE, 0, NULL,FALSE,NULL);
		
		match = add_keyword_to_scanning_table(st, "{", FALSE, FALSE, keyword, contextphp, contextphp, TRUE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, "}", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, TRUE, match, NULL,FALSE,NULL);
		match = add_keyword_to_scanning_table(st, "[", FALSE, FALSE, keyword, contextphp, contextphp, TRUE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, "]", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, TRUE, match, NULL,FALSE,NULL);
		match = add_keyword_to_scanning_table(st, "(", FALSE, FALSE, keyword, contextphp, contextphp, TRUE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, ")", FALSE, FALSE, keyword, contextphp, contextphp, FALSE, TRUE, match, NULL,FALSE,NULL);
		
		add_keyword_to_scanning_table(st, "mysql_affected_rows", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_connect", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_query", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_num_rows", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_fetch_row", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_destroy", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_change_user", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_client_encoding", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_close", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_create_db", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_data_seek", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_db_name", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_db_query", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_drop_db", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_errno", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_error", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_escape_string", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_fetch_array", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_fetch_assoc", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		add_keyword_to_scanning_table(st, "mysql_fetch_field", FALSE, FALSE, function, contextphp, contextphp, FALSE, FALSE, 0, NULL,TRUE,NULL);
		
		add_keyword_to_scanning_table(st, "$[a-z0-9_]+", TRUE, TRUE, variable, contextphp, contextphp, FALSE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, "(//|#)[^\n\t]+", TRUE, TRUE, comment, contextphp, contextphp, FALSE, FALSE, 0, NULL,FALSE,NULL);
		add_keyword_to_scanning_table(st, "[0-9.]+", TRUE, TRUE, value, contextphp, contextphp, FALSE, FALSE, 0, NULL,FALSE,NULL);
		
		add_keyword_to_scanning_table(st, "&[a-z0-9#]+;", TRUE, TRUE, value, context0, context0, FALSE, FALSE, 0, NULL,FALSE,NULL);
	}
#endif
	/* we don't build a automata from regex patterns right now, because I'm not
	very satisfied with my previous attempt to code that.. 
	we build one by hand just to test scanning.. */ 
	

/* for more testing we try some html tags */
/*#define HTMLSTYLEMATCHING*/
#ifdef HTMLSTYLEMATCHING
	g_array_set_size(st->table,39);
	st->table->len = 39;
	g_array_set_size(st->contexts,4);
	st->contexts->len = 4;
	g_array_set_size(st->matches,12);
	st->matches->len = 12; /* match 0 is not used */

	g_array_index(st->contexts, Tcontext, 0).startstate = 0;
	g_array_index(st->contexts, Tcontext, 0).ac = g_completion_new(NULL);
	{
		GList *list = NULL;
		list = g_list_prepend(list, "<img");
		list = g_list_prepend(list, "<i");
		list = g_list_prepend(list, "<!--");
		g_completion_add_items(g_array_index(st->contexts, Tcontext, 0).ac, list);
		g_list_free(list);
	}

	g_array_index(st->table, Ttablerow, 0).row['<'] = 1;
	
	/* comment <!-- */
	g_array_index(st->table, Ttablerow, 1).row['!'] = 2;
	g_array_index(st->table, Ttablerow, 2).row['-'] = 3;
	g_array_index(st->table, Ttablerow, 3).row['-'] = 4;
	g_array_index(st->table, Ttablerow, 4).match = 1;
	g_array_index(st->matches, Tpattern, 1).message = "<!--";
	g_array_index(st->matches, Tpattern, 1).starts_block = TRUE;
	g_array_index(st->matches, Tpattern, 1).selftag = comment;
	g_array_index(st->matches, Tpattern, 1).nextcontext = 1;
	
	/* <img */	
	g_array_index(st->table, Ttablerow, 1).row['i'] = 5;
	g_array_index(st->table, Ttablerow, 5).row['m'] = 6;
	g_array_index(st->table, Ttablerow, 6).row['g'] = 7;
	g_array_index(st->table, Ttablerow, 7).match = 2;
	g_array_index(st->matches, Tpattern, 2).message = "<img";
	g_array_index(st->matches, Tpattern, 2).selftag = braces;
	g_array_index(st->matches, Tpattern, 2).nextcontext = 2;

	/* <i */
	g_array_index(st->table, Ttablerow, 5).match = 3;
	g_array_index(st->matches, Tpattern, 3).message = "<i";
	g_array_index(st->matches, Tpattern, 3).selftag = braces;
	g_array_index(st->matches, Tpattern, 3).nextcontext = 3;

	/* <html */
	g_array_index(st->table, Ttablerow, 1).row['h'] = 8;
	g_array_index(st->table, Ttablerow, 8).row['t'] = 9;
	g_array_index(st->table, Ttablerow, 9).row['m'] = 10;
	g_array_index(st->table, Ttablerow, 10).row['l'] = 11;
	g_array_index(st->table, Ttablerow, 11).match = 4;
	g_array_index(st->matches, Tpattern, 4).message = "<html";
	g_array_index(st->matches, Tpattern, 4).starts_block = TRUE;
	g_array_index(st->matches, Tpattern, 4).selftag = braces;
	
	/* </html> */
	g_array_index(st->table, Ttablerow, 1).row['/'] = 12;
	g_array_index(st->table, Ttablerow, 12).row['h'] = 13;
	g_array_index(st->table, Ttablerow, 13).row['t'] = 14;
	g_array_index(st->table, Ttablerow, 14).row['m'] = 15;
	g_array_index(st->table, Ttablerow, 15).row['l'] = 16;
	g_array_index(st->table, Ttablerow, 16).row['>'] = 17;
	g_array_index(st->table, Ttablerow, 17).match = 5;
	g_array_index(st->matches, Tpattern, 5).message = "</html>";
	g_array_index(st->matches, Tpattern, 5).ends_block = TRUE;
	g_array_index(st->matches, Tpattern, 5).selftag = braces;
	g_array_index(st->matches, Tpattern, 5).blockstartpattern = 4;
	
	/* <body */ 
	g_array_index(st->table, Ttablerow, 1).row['b'] = 18;
	g_array_index(st->table, Ttablerow, 18).row['o'] = 19;
	g_array_index(st->table, Ttablerow, 19).row['d'] = 20;
	g_array_index(st->table, Ttablerow, 20).row['y'] = 21;
	g_array_index(st->table, Ttablerow, 21).row['>'] = 22;
	g_array_index(st->table, Ttablerow, 22).match = 6;
	g_array_index(st->matches, Tpattern, 6).message = "<body";
	g_array_index(st->matches, Tpattern, 6).starts_block = TRUE;
	g_array_index(st->matches, Tpattern, 6).selftag = braces;

	/* </body> starts with a / on state 12 */
	g_array_index(st->table, Ttablerow, 12).row['b'] = 23;
	g_array_index(st->table, Ttablerow, 23).row['o'] = 24;
	g_array_index(st->table, Ttablerow, 24).row['d'] = 25;
	g_array_index(st->table, Ttablerow, 25).row['y'] = 26;
	g_array_index(st->table, Ttablerow, 26).row['>'] = 27;
	g_array_index(st->table, Ttablerow, 27).match = 7;
	g_array_index(st->matches, Tpattern, 7).message = "</body>";
	g_array_index(st->matches, Tpattern, 7).ends_block = TRUE;
	g_array_index(st->matches, Tpattern, 7).selftag = braces;
	g_array_index(st->matches, Tpattern, 7).blockstartpattern = 6;

	/* context for comment --> */
	g_array_index(st->contexts, Tcontext, 1).startstate = 28;
	g_array_index(st->table, Ttablerow, 28).row['-'] = 29;
	g_array_index(st->table, Ttablerow, 29).row['-'] = 30;
	g_array_index(st->table, Ttablerow, 30).row['>'] = 31;
	g_array_index(st->table, Ttablerow, 31).match = 8;
	g_array_index(st->matches, Tpattern, 8).message = "-->";
	g_array_index(st->matches, Tpattern, 8).nextcontext = 0;
	g_array_index(st->matches, Tpattern, 8).ends_block = TRUE;
	g_array_index(st->matches, Tpattern, 8).blockstartpattern = 1;
	g_array_index(st->matches, Tpattern, 8).blocktag = comment;
	g_array_index(st->matches, Tpattern, 8).selftag = comment;

	/* closure > in context for <img */
	g_array_index(st->contexts, Tcontext, 2).startstate = 32;
	g_array_index(st->contexts, Tcontext, 2).ac = g_completion_new(NULL);
	{
		GList *list = NULL;
		list = g_list_prepend(list, "src=");
		list = g_list_prepend(list, "width=");
		list = g_list_prepend(list, "height=");
		g_completion_add_items(g_array_index(st->contexts, Tcontext, 2).ac, list);
		g_list_free(list);
	}
	g_array_index(st->table, Ttablerow, 32).row['>'] = 33;
	g_array_index(st->table, Ttablerow, 33).match = 9;
	g_array_index(st->matches, Tpattern, 9).message = ">";
	g_array_index(st->matches, Tpattern, 9).nextcontext = 0;
	g_array_index(st->matches, Tpattern, 9).selftag = braces;
	/* src= in context for <img */
	g_array_index(st->table, Ttablerow, 32).row['s'] = 33;
	g_array_index(st->table, Ttablerow, 33).row['r'] = 34;
	g_array_index(st->table, Ttablerow, 34).row['c'] = 35;
	g_array_index(st->table, Ttablerow, 35).row['='] = 36;
	g_array_index(st->table, Ttablerow, 35).row[' '] = 35;
	g_array_index(st->table, Ttablerow, 35).row['\t'] = 35;
	g_array_index(st->table, Ttablerow, 35).row['\n'] = 35;
	g_array_index(st->table, Ttablerow, 35).row['\r'] = 35;
	g_array_index(st->table, Ttablerow, 36).match = 10;
	g_array_index(st->matches, Tpattern, 10).message = ">";
	g_array_index(st->matches, Tpattern, 10).nextcontext = 2;
	g_array_index(st->matches, Tpattern, 10).selftag = storage;
	/* closure > in context for <i */
	g_array_index(st->contexts, Tcontext, 3).startstate = 37;
	g_array_index(st->table, Ttablerow, 37).row['>'] = 38;
	g_array_index(st->table, Ttablerow, 38).match = 11;
	g_array_index(st->matches, Tpattern, 11).message = ">";
	g_array_index(st->matches, Tpattern, 11).nextcontext = 0;
	g_array_index(st->matches, Tpattern, 11).selftag = braces;

#endif /* #ifdef HTMLSTYLEMATCHING */

	print_scantable_stats(st);
	return st;
}
#endif /* HARDCODED PATTERNS */
