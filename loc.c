#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 4096

static const char *base_path = NULL;
static size_t base_len = 0;


typedef struct {
	long empty;
	long comment;
	long code;
} Counts;

/* ---------- utility ---------- */

int is_text_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	
	unsigned char buf[512];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	
	for (size_t i = 0; i < n; i++) {
		if (buf[i] == 0) return 0; /* binary */
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
				break; /* rest of line is comment */
			}
			
			if (!isspace((unsigned char)*p)) {
				code_seen = 1;
				comment_only = 0;
			}
			p++;
		}
		
		if (in_block_comment && comment_only) {
			local.comment++;
		} else if (code_seen) {
			local.code++;
		} else {
			local.comment++;
		}
	}
	
	fclose(f);
	
	const char *out = path;
	
	/* strip base path */
	if (strncmp(path, base_path, base_len) == 0) {
		out = path + base_len;
		if (*out == '/')
			out++;
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
	while ((ent = readdir(dir))) {
		/* ignore hidden files & folders */
		if (ent->d_name[0] == '.')
			continue;
		
		char full[4096];
		snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
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
	
	/* remove trailing slash */
	if (base_len > 1 && base_path[base_len - 1] == '/')
		base_len--;
	
	Counts total = {0, 0, 0};
	walk(argv[1], &total);
	
	printf("\nTOTAL:\n");
	printf("e-%ld cs-%ld co-%ld\n",
		   total.empty, total.comment, total.code);
	return 0;
}

