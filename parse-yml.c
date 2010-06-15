#include <stdio.h>
#include <string.h>
#include <ctype.h>

struct YMLParserContext_;

typedef int (*YMLParserSectionHandler)(struct YMLParserContext_ *yml_parser_context, char *line);

typedef struct YMLParserSection_ {
	const char *section_name;
	YMLParserSectionHandler handler;	
} YMLParserSection;

typedef struct YMLParserContext_ {
	YMLParserSection *current_section;
} YMLParserContext;

typedef struct YMLParserStrPair_ {
	char *str1;
	char *str2;
} YMLParserStrPair;

static char *skip_spaces(const char *str)
{
	while (isspace((unsigned char) *str)) {
		str++;
	}
	return (char *) str;
}

static char *chomp(char *str)
{
	char *pnt;
	
	if ((pnt = strrchr(str, '\n')) != NULL) {
		*pnt = 0;
	}
	if ((pnt = strrchr(str, '\r')) != NULL) {
		*pnt = 0;
	}
	return str;
}

static int yml_parser_get_str_pair(YMLParserStrPair *str_pair, char *str)
{
	char *str1;
	char *str2;
	char *pnt;
	
	str_pair->str1 = str_pair->str2 = NULL;
	str1 = skip_spaces(str);
	if (*str1 == 0) {
		return -1;
	}
	if ((pnt = strchr(str1, ':')) == NULL) {
		return -1;
	}
	str2 = skip_spaces(pnt + 1);
	if (*str2 == 0) {
		return -1;
	}
	*pnt-- = 0;
	while (isspace((unsigned char) *pnt)) {
		*pnt-- = 0;
	}
	pnt = str2 + strlen(str2) - 1U;
	while (isspace((unsigned char) *pnt)) {
		*pnt-- = 0;
	}
	if (*str1 == 0 || *str2 == 0) {
		return -1;
	}
	str_pair->str1 = str1;
	str_pair->str2 = str2;
	
	return 0;
}

char *yml_parser_get_str(char *line)
{
	char *pnt;
	char *str;
	
	str = skip_spaces(line);
	if (*str == 0) {
		return NULL;
	}
	pnt = str + strlen(str) - 1U;
	while (isspace((unsigned char) *pnt)) {
		*pnt-- = 0;
	}
	if (*pnt == 0) {
		return NULL;
	}
	return str;
}

int yml_parser_get_bool(const char *str)
{
	if (strcasecmp(str, "1") == 0 || strcasecmp(str, "true") ||
		strcasecmp(str, "oui") == 0 || strcasecmp(str, "yes") == 0) {
		return 1;
	}
	if (strcasecmp(str, "0") == 0 || strcasecmp(str, "false") ||
		strcasecmp(str, "non") == 0 || strcasecmp(str, "no") == 0) {
		return 0;
	}
	return -1;
}

static int yml_parser_section_raccourcis(YMLParserContext *yml_parser_context, char *line)
{
	YMLParserStrPair str_pair;
	
	if (yml_parser_get_str_pair(&str_pair, line) != 0) {
		return -1;
	}
	
	printf("[%s] [%s]\n", str_pair.str1, str_pair.str2);
	
	return 0;
}

static int yml_parser_section_lancement(YMLParserContext *yml_parser_context, char *line)
{
	YMLParserStrPair str_pair;
	char *str;
	
	if ((str = yml_parser_get_str(line)) == NULL) {
		return -1;
	}
	printf("[%s]\n", str);
	
	return 0;
}

static int yml_parser_section_mortscript(YMLParserContext *yml_parser_context, char *line)
{
	YMLParserStrPair str_pair;
	char *str;
	
	if ((str = yml_parser_get_str(line)) == NULL) {
		return -1;
	}
	printf("[%s]\n", str);
	
	return 0;
}

static int yml_parser_section_reglages(YMLParserContext *yml_parser_context, char *line)
{
	YMLParserStrPair str_pair;
	
	if (yml_parser_get_str_pair(&str_pair, line) != 0) {
		return -1;
	}
	
	printf("[%s] [%s]\n", str_pair.str1, str_pair.str2);
	
	return 0;
}

YMLParserSection yml_parser_sections[] = {
	{ "Raccourcis", yml_parser_section_raccourcis },
	{ "Lancement", yml_parser_section_lancement },
	{ "Mortscript", yml_parser_section_mortscript },
	{ "Reglages", yml_parser_section_reglages },
	{ NULL, NULL }
};

static int yml_parser_change_section(YMLParserContext *yml_parser_context, char *line)
{
	char *pnt;
	YMLParserSection *scanned_yml_parser_section = yml_parser_sections;
	
	if ((pnt = strrchr(line, ':')) == NULL) {
		return -1;
	}
	*pnt-- = 0;
	while (pnt != line) {
		if (!isspace((unsigned char) *pnt)) {
			break;
		}
		*pnt-- = 0;
	}
	while (scanned_yml_parser_section->section_name != NULL) {
		if (strcasecmp(line, scanned_yml_parser_section->section_name) == 0) {
			yml_parser_context->current_section = scanned_yml_parser_section;
			return 0;
		}
		scanned_yml_parser_section++;
	}
	return -1;
}

static int yml_parser(const char *file)
{	
	char line[4096];
	YMLParserContext yml_parser_context;
	FILE *fp;
	char *pnt;
	
	yml_parser_context.current_section = NULL;	
	if ((fp = fopen(file, "rt")) == NULL) {
		return -1;
	}
	while (fgets(line, sizeof line, fp) != NULL) {
		pnt = skip_spaces(line);
		if (*pnt == '#') {
			continue;
		}
		chomp(pnt);
		if (*pnt == 0) {
			continue;
		}
		if (!isspace(*line)) {
			if (yml_parser_change_section(&yml_parser_context, line) != 0) {
				yml_parser_context.current_section = NULL;
			}
			continue;			
		}
		if (yml_parser_context.current_section == NULL ||
			yml_parser_context.current_section->handler == NULL) {
			continue;
		}
		yml_parser_context.current_section->handler(&yml_parser_context, line);
	}
	fclose(fp);
	
	return 0;
}

int main(void)
{
	yml_parser("debridage.ini");
	
	return 0;
}