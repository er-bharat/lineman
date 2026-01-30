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

/* ---------- globals ---------- */

static FileType types[MAX_TYPES];
static int type_count = 0;

/* ---------- utility ---------- */

int is_text_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	
	unsigned char buf[512];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	
	for (size_t i = 0; i < n; i++) {
		if (buf[i] == 0)
			return 0; /* binary */
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

void get_dirname(const char *path, char *out, size_t out_size) {
	strncpy(out, path, out_size - 1);
	out[out_size - 1] = 0;
	
	char *slash = strrchr(out, '/');
	if (slash)
		*slash = 0;
	else
		strcpy(out, ".");
}

/* ---------- line counting ---------- */

void count_file(const char *path, Counts *total) {
	if (!is_text_file(path))
		return;
	
	FILE *f = fopen(path, "r");
	if (!f) return;
	
	Counts local = {0, 0, 0};
	char line[MAX_LINE];
	int in_block_comment = 0;
	
	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		
		if (is_blank(p)) {
			local.empty++;
			continue;
		}
		
		int code_seen = 0;
		int comment_only = 1;
		
		while (*p) {
			if (in_block_comment) {
				if (p[0] == '*' && p[1] == '/') {
					in_block_comment = 0;
					p += 2;
				} else {
					p++;
				}
				continue;
			}
			
			if (p[0] == '/' && p[1] == '*') {
				in_block_comment = 1;
				p += 2;
				continue;
			}
			
			if (p[0] == '/' && p[1] == '/') {
				break;
			}
			
			if (!isspace((unsigned char)*p)) {
				code_seen = 1;
				comment_only = 0;
			}
			p++;
		}
		
		if (in_block_comment && comment_only)
			local.comment++;
		else if (code_seen)
			local.code++;
		else
			local.comment++;
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
		if (last_dir[0] != 0) {
			printf("\n");   /* separate folders */
		}
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
	
	/* ---------- FIRST PASS: files ---------- */
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.')
			continue;
		
		char full[4096];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
		
		if (stat(full, &st) == 0 && S_ISREG(st.st_mode)) {
			count_file(full, total);
		}
	}
	
	/* rewind directory for second pass */
	rewinddir(dir);
	
	/* ---------- SECOND PASS: subdirectories ---------- */
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.')
			continue;
		
		char full[4096];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
		
		if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
			walk(full, total);
		}
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
	for (int i = 0; i < type_count; i++) {
		printf("%-6s %ld\n", types[i].ext, types[i].count);
	}
	
	printf("\nTOTAL:\n");
	printf("e-%ld cs-%ld co-%ld\n",
		   total.empty, total.comment, total.code);
	
	return 0;
}
