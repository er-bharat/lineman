/*
 * author: bharat kumar
 * 
 * https://github.com/er-bharat/lineman.git
 * 
 * locscan — source code line analyzer
 *
 * Counts empty (e-), comment (cs-), and code (co-) lines.
 *
 * Build:
 *   gcc -std=c11 -Wall -Wextra -O2 lineman.c -o lineman
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ============================================================
 *  Configuration
 * ============================================================ */

#define MAX_LINE_LEN           4096
#define MAX_PATH_LEN           4096
#define MAX_EXT_LEN            16
#define MAX_FILE_TYPES         64
#define IO_BUFFER_SIZE         512

#define MIN_PATH_COLUMN_WIDTH  20
#define MAX_PATH_COLUMN_WIDTH  80

/* ============================================================
 *  Data structures
 * ============================================================ */

typedef struct {
	long empty;
	long comment;
	long code;
} LineCounts;

typedef struct {
	char ext[MAX_EXT_LEN];
	long count;
} FileTypeCount;

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
} LineKind;

/* ============================================================
 *  Global state
 * ============================================================ */
static int suppress_file_output = 0;

static FileTypeCount file_types[MAX_FILE_TYPES];
static int file_type_count = 0;

static char last_dir[MAX_PATH_LEN] = "";

static const char *base_path = NULL;
static size_t base_len = 0;

static int path_column_width = 0;

/* ============================================================
 *  Comment rules
 * ============================================================ */

static CommentRule rules[] = {
	{ "c",     "//", "/*", "*/" },
	{ "cpp",   "//", "/*", "*/" },
	{ "cs",    "//", "/*", "*/" },
	{ "go",    "//", "/*", "*/" },
	{ "java",  "//", "/*", "*/" },
	{ "js",    "//", "/*", "*/" },
	{ "kt",    "//", "/*", "*/" },
	{ "php",   "//", "/*", "*/" },
	{ "rs",    "//", "/*", "*/" },
	{ "swift", "//", "/*", "*/" },
	{ "ts",    "//", "/*", "*/" },
	
	{ "py",    "#",  NULL, NULL },
	{ "rb",    "#",  "=begin", "=end" },
	{ "sh",    "#",  NULL, NULL },
	{ "r",     "#",  NULL, NULL },
	{ "yml",   "#",  NULL, NULL },
	{ "txt",   "#",  NULL, NULL },
	
	{ "sql",   "--", "/*", "*/" },
	{ "lua",   "--", "--[[", "]]" },
	
	{ "html",  NULL, "<!--", "-->" },
	{ "xml",   NULL, "<!--", "-->" },
	{ "svg",   NULL, "<!--", "-->" }
};

#define RULE_COUNT (sizeof(rules) / sizeof(rules[0]))

static CommentRule fallback_rule = {
	NULL, "//", "/*", "*/"
};

/* ============================================================
 *  Utility helpers
 * ============================================================ */

static int is_blank(const char *s) {
	while (*s) {
		if (!isspace((unsigned char)*s)) {
			return 0;
		}
		s++;
	}
	return 1;
}

static int is_text_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return 0;
	}
	
	unsigned char buf[IO_BUFFER_SIZE];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	
	for (size_t i = 0; i < n; i++) {
		if (buf[i] == 0) {
			return 0;
		}
	}
	return 1;
}

static void get_dirname(const char *path, char *out, size_t out_size) {
	strncpy(out, path, out_size - 1);
	out[out_size - 1] = '\0';
	
	char *slash = strrchr(out, '/');
	if (slash) {
		*slash = '\0';
	} else {
		strcpy(out, ".");
	}
}

/* ============================================================
 *  Terminal width handling
 * ============================================================ */

static int get_terminal_width(void) {
	struct winsize ws;
	
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		return ws.ws_col;
	}
	
	return 100;
}

static int compute_path_column_width(void) {
	int term_width = get_terminal_width();
	
	const int stats_width =
	2 +
	4 + 6 +
	2 +
	5 + 6 +
	2 +
	4 + 6;
	
	int available = term_width - stats_width;
	
	if (available < MIN_PATH_COLUMN_WIDTH) {
		return MIN_PATH_COLUMN_WIDTH;
	}
	
	if (available > MAX_PATH_COLUMN_WIDTH) {
		return MAX_PATH_COLUMN_WIDTH;
	}
	
	return available;
}

static void print_path_column(const char *path) {
	size_t len = strlen(path);
	
	if ((int)len <= path_column_width) {
		printf("%-*s", path_column_width, path);
		return;
	}
	
	printf("...%s", path + (len - (path_column_width - 3)));
}

/* ============================================================
 *  File type tracking
 * ============================================================ */

static void count_file_type(const char *path) {
	const char *dot = strrchr(path, '.');
	if (!dot || dot == path) {
		return;
	}
	
	char ext[MAX_EXT_LEN];
	strncpy(ext, dot + 1, sizeof(ext) - 1);
	ext[sizeof(ext) - 1] = '\0';
	
	for (char *p = ext; *p; p++) {
		*p = (char)tolower((unsigned char)*p);
	}
	
	for (int i = 0; i < file_type_count; i++) {
		if (strcmp(file_types[i].ext, ext) == 0) {
			file_types[i].count++;
			return;
		}
	}
	
	if (file_type_count < MAX_FILE_TYPES) {
		strcpy(file_types[file_type_count].ext, ext);
		file_types[file_type_count].count = 1;
		file_type_count++;
	}
}

/* ============================================================
 *  Comment rule lookup
 * ============================================================ */

static const CommentRule *get_rule_for_file(const char *path) {
	const char *dot = strrchr(path, '.');
	if (!dot || dot == path) {
		return &fallback_rule;
	}
	
	char ext[MAX_EXT_LEN];
	strncpy(ext, dot + 1, sizeof(ext) - 1);
	ext[sizeof(ext) - 1] = '\0';
	
	for (char *p = ext; *p; p++) {
		*p = (char)tolower((unsigned char)*p);
	}
	
	for (size_t i = 0; i < RULE_COUNT; i++) {
		if (strcmp(rules[i].ext, ext) == 0) {
			return &rules[i];
		}
	}
	
	return &fallback_rule;
}

/* ============================================================
 *  Line classification
 * ============================================================ */

static LineKind classify_line(
	char *line,
	int *in_block,
	const CommentRule *r
) {
	char *p = line;
	
	if (is_blank(p)) {
		return LINE_EMPTY;
	}
	
	int code_seen = 0;
	
	while (*p) {
		if (*in_block) {
			if (r->block_end &&
				strncmp(p, r->block_end, strlen(r->block_end)) == 0) {
				*in_block = 0;
			p += strlen(r->block_end);
			continue;
				}
				p++;
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
				
				if (!isspace((unsigned char)*p)) {
					code_seen = 1;
				}
				
				p++;
	}
	
	if (*in_block) {
		return LINE_COMMENT;
	}
	
	return code_seen ? LINE_CODE : LINE_COMMENT;
}

/* ============================================================
 *  File analysis
 * ============================================================ */

static void count_file(const char *path, LineCounts *total) {
	if (!is_text_file(path)) {
		return;
	}
	
	FILE *f = fopen(path, "r");
	if (!f) {
		return;
	}
	
	const CommentRule *rule = get_rule_for_file(path);
	
	LineCounts local = { 0, 0, 0 };
	char line[MAX_LINE_LEN];
	int in_block = 0;
	
	while (fgets(line, sizeof(line), f)) {
		LineKind kind = classify_line(line, &in_block, rule);
		
		if (kind == LINE_EMPTY)   local.empty++;
		if (kind == LINE_COMMENT) local.comment++;
		if (kind == LINE_CODE)    local.code++;
	}
	
	fclose(f);
	
	count_file_type(path);
	
	const char *out = path;
	if (strncmp(path, base_path, base_len) == 0) {
		out = path + base_len;
		if (*out == '/') {
			out++;
		}
	}
	
	char curr_dir[MAX_PATH_LEN];
	get_dirname(out, curr_dir, sizeof(curr_dir));
	
	if (strcmp(curr_dir, last_dir) != 0) {
		if (last_dir[0]) {
			printf("\n");
		}
		strcpy(last_dir, curr_dir);
	}
	
	if (!suppress_file_output) {
		print_path_column(out);
		printf("  e-%6ld  cs-%6ld  co-%6ld\n",
			   local.empty,
		 local.comment,
		 local.code);
	}
	
	total->empty   += local.empty;
	total->comment += local.comment;
	total->code    += local.code;
}

/* ============================================================
 *  Directory traversal
 * ============================================================ */

static void walk(const char *path, LineCounts *total) {
	struct stat st;
	if (stat(path, &st) != 0) {
		return;
	}
	
	if (S_ISREG(st.st_mode)) {
		count_file(path, total);
		return;
	}
	
	if (!S_ISDIR(st.st_mode)) {
		return;
	}
	
	DIR *dir = opendir(path);
	if (!dir) {
		return;
	}
	
	struct dirent *ent;
	
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.') {
			continue;
		}
		
		char full[MAX_PATH_LEN];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
		
		if (stat(full, &st) == 0 && S_ISREG(st.st_mode)) {
			count_file(full, total);
		}
	}
	
	rewinddir(dir);
	
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.') {
			continue;
		}
		
		char full[MAX_PATH_LEN];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
		
		if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
			walk(full, total);
		}
	}
	
	closedir(dir);
}

static void print_separator(void) {
	int stats_width =
	2 +
	4 + 6 +
	2 +
	5 + 6 +
	2 +
	4 + 6;
	
	int total_width = path_column_width + stats_width;
	
	for (int i = 0; i < total_width; i++)
		putchar('-');
	
	putchar('\n');
}


/* ============================================================
 *  Entry point
 * ============================================================ */

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <file|directory>\n", argv[0]);
		return 1;
	}
	
	base_path = argv[1];
	base_len = strlen(base_path);
	if (base_len > 1 && base_path[base_len - 1] == '/')
		base_len--;
	
	path_column_width = compute_path_column_width();
	
	LineCounts total = {0, 0, 0};
	
	/* ---------- PASS 1: collect stats only ---------- */
	suppress_file_output = 1;
	walk(argv[1], &total);
	
	/* ---------- FILE TYPES (TOP) ---------- */
	printf("FILES BY TYPE:\n");
	for (int i = 0; i < file_type_count; i++) {
		printf("  %-8s %ld\n",
			   file_types[i].ext,
		 file_types[i].count);
	}
	
	printf("\n");
	
	/* ---------- PASS 2: print files ---------- */
	suppress_file_output = 0;
	last_dir[0] = '\0';   /* reset grouping */
	walk(argv[1], &(LineCounts){0,0,0}); /* discard recount */
	
	/* ---------- TOTAL (BOTTOM) ---------- */
	printf("\n");
	print_separator();
	
	print_path_column("TOTAL");
	printf("  e-%6ld  cs-%6ld  co-%6ld\n",
		   total.empty,
		total.comment,
		total.code);
	
	return 0;
}

