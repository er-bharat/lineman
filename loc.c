#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 4096
#define MAX_TYPES 64

static char last_dir[4096] = "";
static const char *base_path = NULL;
static size_t base_len = 0;

/* ---------- structs ---------- */

typedef struct {
	long empty;
	long comment;
	long code;
} Counts;

typedef struct {
	char ext[16];
	long count;
} FileType;

typedef struct {
	const char *ext;
	const char *line;
	const char *block_start;
	const char *block_end;
} CommentRule;

typedef enum {
	LINE_EMPTY,
	LINE_COMMENT,
	LINE_CODE
} LineType;

/* ---------- globals ---------- */

static FileType types[MAX_TYPES];
static int type_count = 0;

/* ---------- comment rules ---------- */

static CommentRule rules[] = {
	{ "c",   "//", "/*", "*/" },
	{ "h",   "//", "/*", "*/" },
	{ "cpp", "//", "/*", "*/" },
	{ "qml", "//", "/*", "*/" },
	{ "js",  "//", "/*", "*/" },
	{ "java","//", "/*", "*/" },
	{ "py",  "#",  NULL, NULL },
	{ "sh",  "#",  NULL, NULL },
};

#define RULE_COUNT (sizeof(rules) / sizeof(rules[0]))

static CommentRule general_rule = {
	NULL, "//", "/*", "*/"
};

/* ---------- utility ---------- */

int is_text_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	
	unsigned char buf[512];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	
	for (size_t i = 0; i < n; i++) {
		if (buf[i] == 0)
			return 0;
	}
	return 1;
}

int is_blank(const char *s) {
	while (*s) {
		if (!isspace((unsigned char)*s))
			return 0;
		s++;
	}
	return 1;
}

/* ---------- file type counting ---------- */

void count_file_type(const char *path) {
	const char *dot = strrchr(path, '.');
	if (!dot || dot == path)
		return;
	
	char ext[16];
	strncpy(ext, dot + 1, sizeof(ext) - 1);
	ext[sizeof(ext) - 1] = 0;
	
	for (char *p = ext; *p; p++)
		*p = tolower((unsigned char)*p);
	
	for (int i = 0; i < type_count; i++) {
		if (strcmp(types[i].ext, ext) == 0) {
			types[i].count++;
			return;
		}
	}
	
	if (type_count < MAX_TYPES) {
		strcpy(types[type_count].ext, ext);
		types[type_count].count = 1;
		type_count++;
	}
}

/* ---------- path helpers ---------- */

void get_dirname(const char *path, char *out, size_t out_size) {
	strncpy(out, path, out_size - 1);
	out[out_size - 1] = 0;
	
	char *slash = strrchr(out, '/');
	if (slash)
		*slash = 0;
	else
		strcpy(out, ".");
}

/* ---------- comment rule lookup ---------- */

const CommentRule *get_rule_for_file(const char *path) {
	const char *dot = strrchr(path, '.');
	if (!dot || dot == path)
		return &general_rule;
	
	char ext[16];
	strncpy(ext, dot + 1, sizeof(ext) - 1);
	ext[sizeof(ext) - 1] = 0;
	
	for (char *p = ext; *p; p++)
		*p = tolower((unsigned char)*p);
	
	for (size_t i = 0; i < RULE_COUNT; i++) {
		if (strcmp(rules[i].ext, ext) == 0)
			return &rules[i];
	}
	
	return &general_rule;
}

/* ---------- line classifier ---------- */

LineType classify_line(char *line, int *in_block, const CommentRule *r) {
	char *p = line;
	
	if (is_blank(p))
		return LINE_EMPTY;
	
	int code_seen = 0;
	
	while (*p) {
		if (*in_block) {
			if (r->block_end &&
				strncmp(p, r->block_end, strlen(r->block_end)) == 0) {
				*in_block = 0;
			p += strlen(r->block_end);
				} else {
					p++;
				}
				continue;
		}
		
		if (r->block_start &&
			strncmp(p, r->block_start, strlen(r->block_start)) == 0) {
			*in_block = 1;
		p += strlen(r->block_start);
		continue;
			}
			
			if (r->line &&
				strncmp(p, r->line, strlen(r->line)) == 0) {
				return code_seen ? LINE_CODE : LINE_COMMENT;
				}
				
				if (!isspace((unsigned char)*p))
					code_seen = 1;
		
		p++;
	}
	
	if (*in_block)
		return LINE_COMMENT;
	
	return code_seen ? LINE_CODE : LINE_COMMENT;
}

/* ---------- file processing ---------- */

void count_file(const char *path, Counts *total) {
	if (!is_text_file(path))
		return;
	
	FILE *f = fopen(path, "r");
	if (!f) return;
	
	const CommentRule *rule = get_rule_for_file(path);
	
	Counts local = {0, 0, 0};
	char line[MAX_LINE];
	int in_block = 0;
	
	while (fgets(line, sizeof(line), f)) {
		LineType t = classify_line(line, &in_block, rule);
		
		if (t == LINE_EMPTY)   local.empty++;
		if (t == LINE_COMMENT) local.comment++;
		if (t == LINE_CODE)    local.code++;
	}
	
	fclose(f);
	
	count_file_type(path);
	
	const char *out = path;
	if (strncmp(path, base_path, base_len) == 0) {
		out = path + base_len;
		if (*out == '/')
			out++;
	}
	
	char curr_dir[4096];
	get_dirname(out, curr_dir, sizeof(curr_dir));
	
	if (strcmp(curr_dir, last_dir) != 0) {
		if (last_dir[0])
			printf("\n");
		strcpy(last_dir, curr_dir);
	}
	
	printf("%s | e-%ld cs-%ld co-%ld\n",
		   out, local.empty, local.comment, local.code);
	
	total->empty   += local.empty;
	total->comment += local.comment;
	total->code    += local.code;
}

/* ---------- directory walk ---------- */

void walk(const char *path, Counts *total) {
	struct stat st;
	if (stat(path, &st) != 0)
		return;
	
	if (S_ISREG(st.st_mode)) {
		count_file(path, total);
		return;
	}
	
	if (!S_ISDIR(st.st_mode))
		return;
	
	DIR *dir = opendir(path);
	if (!dir) return;
	
	struct dirent *ent;
	
	/* files first */
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.')
			continue;
		
		char full[4096];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
		
		if (stat(full, &st) == 0 && S_ISREG(st.st_mode))
			count_file(full, total);
	}
	
	rewinddir(dir);
	
	/* subdirs next */
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.')
			continue;
		
		char full[4096];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
		
		if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
			walk(full, total);
	}
	
	closedir(dir);
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <file|directory>\n", argv[0]);
		return 1;
	}
	
	base_path = argv[1];
	base_len = strlen(base_path);
	if (base_len > 1 && base_path[base_len - 1] == '/')
		base_len--;
	
	Counts total = {0, 0, 0};
	
	walk(argv[1], &total);
	
	printf("\nFILES BY TYPE:\n");
	for (int i = 0; i < type_count; i++)
		printf("%-6s %ld\n", types[i].ext, types[i].count);
	
	printf("\nTOTAL:\n");
	printf("e-%ld cs-%ld co-%ld\n",
		   total.empty, total.comment, total.code);
	
	return 0;
}
