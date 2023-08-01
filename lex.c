typedef struct Lexer Lexer;
typedef struct DebugInfo DebugInfo;

struct DebugInfo {
	size_t line, col;
	char *start, *end;
};

enum Token {
	TOKEN_INVALID = 256,
#define X(val) TOKEN_##val,
	INSTRUCTRIONS
#undef X
	TOKEN_BINLIT,
	TOKEN_HEXLIT,
	TOKEN_OCTLIT,
	TOKEN_FLOATLIT,
	TOKEN_INTLIT,
	TOKEN_STRLIT,
};
typedef enum Token Token;

const char *tokenDebug[] = {
	"INVALID",
#define X(val) #val,
	INSTRUCTRIONS
#undef X
	"BINLIT",
	"HEXLIT",
	"OCTLIT",
	"FLOATLIT",
	"INTLIT",
	"STRLIT",
};

char *keywords[] = {
	"________________________________",
#define X(val) #val,
	INSTRUCTRIONS
#undef X
};

size_t keywordLengths[] = {
	0,
#define X(val) STRLEN(#val),
	INSTRUCTRIONS
#undef X
};

struct Lexer {
	char **keywords;
	size_t keywordsCount;
	size_t *keywordLengths;
	char *src, *cur, *srcEnd;
	char *start, *end;
	Token token;
	bool eof;
	DebugInfo debugInfo;
};

void lex(Lexer *l) {
	char *s = NULL;
	int check = 0;

	if(l->cur >= l->srcEnd) {
		l->eof = true;
		l->token = TOKEN_INVALID;
		return;
	}

	while(true) {
		while(isspace(*l->cur)) {
			if(*l->cur == '\n') {
				l->debugInfo.col = 0;
				++l->debugInfo.line;
			} else {
				++l->debugInfo.col;
			}
			++l->cur;
		}

		if(l->cur[0] == '/' && l->cur[1] == '/') {
			l->debugInfo.col = 0;
			++l->debugInfo.line;
			while(*(l->cur++) != '\n');
			//++l->cur;
			continue;
		}

		if(l->cur[0] == '/' && l->cur[1] == '*') {
			l->cur += 2;
			l->debugInfo.col += 2;
			while(l->cur[0] != '*' || l->cur[1] != '/') {
				if(*l->cur == '\n') {
					l->debugInfo.col = 0;
					++l->debugInfo.line;
				} else {
					++l->debugInfo.col;
				}
				++l->cur;
			}
			l->cur += 2;
			l->debugInfo.col += 2;
			continue;
		}

		break;
	}

	if(l->cur >= l->debugInfo.end) {
		for(s = l->cur; *s && *s != '\n'; ++s);
		l->debugInfo.start = l->cur;
		l->debugInfo.end = s;
	}

	if(!*l->cur) {
		l->eof = true;
		l->token = TOKEN_INVALID;
		return;
	}

	for(size_t i = 0; i < l->keywordsCount; ++i) {
		if(l->cur + l->keywordLengths[i] >= l->srcEnd)
			continue;

		check = l->cur[l->keywordLengths[i]]; 

		if(strstr(l->cur, l->keywords[i]) == l->cur &&
				(isspace(check) ||
				 check == ';' ||
				 check == ',' ||
				 check == '.' ||
				 check == '{' ||
				 check == '}' ||
				 check == '(' ||
				 check == ')'))
		{
			l->start = l->cur;
			l->cur += l->keywordLengths[i];
			l->end = l->cur;
			l->debugInfo.col += l->keywordLengths[i];
			l->token = i+TOKEN_INVALID;
			return;
		}
	}

	const bool firstiszero = (l->cur[0] == '0');

	if(firstiszero && (l->cur[1] == 'b' || l->cur[1] == 'B')) {
		for(s = l->cur + 2; *s == '0' || *s == '1'; ++s);
		l->start = l->cur;
		l->end = s;
		l->token = TOKEN_BINLIT;
		l->cur = s;
	} else if(firstiszero && (l->cur[1] == 'x' || l->cur[1] == 'X')) {
		for(s = l->cur + 2; isdigit(*s) || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'); ++s);
		l->start = l->cur;
		l->end = s;
		l->token = TOKEN_HEXLIT;
		l->cur = s;
	} else if(firstiszero && (isdigit(l->cur[1]))) {
		for(s = l->cur + 1; *s >= '0' && *s <= '7'; ++s);
		l->start = l->cur;
		l->end = s;
		l->token = TOKEN_OCTLIT;
		l->cur = s;
	} else if(isdigit(l->cur[0])) {
		for(s = l->cur; isdigit(*s); ++s);
		if(*s == '.') {
			while(isdigit(*++s));
			l->start = l->cur;
			l->end = s;
			l->token = TOKEN_FLOATLIT;
			l->cur = s;
		} else {
			l->start = l->cur;
			l->end = s;
			l->token = TOKEN_INTLIT;
			l->cur = s;
		}
	} else if(l->cur[0] == '"') {
		for(s = l->cur + 1; *s != '"'; ++s)
			if(*s == '\\')
				++s;

		l->start = l->cur;
		l->end = s + 1;
		l->token = TOKEN_STRLIT;
		l->cur = s;
	} else {
		*(l->debugInfo.end) = 0;
		error(1, 0, "bad token at line %zu, col %zu\n>\t%s",
				l->debugInfo.line, l->debugInfo.col, l->debugInfo.start);
	}
}
