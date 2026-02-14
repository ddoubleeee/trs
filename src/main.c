#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <dirent.h>

#define XDG_X11_NAME "x11"
#define XDG_WAYLAND_NAME "wayland"
#define X11_COPY_CMD "xclip -selection clipboard"
#define WAYLAND_COPY_CMD "wl-copy"
#define DICT_FILE_EXTENSION ".dict"
#define MAX_DICT_LINE_LEN 1024
#define MAX_INPUT_LEN 1024
#define MAX_RES_LEN 1024
#define MAX_DICT_PATH_LEN 1024
#define CLIPBOARD_SIZE 4096
#define MAX_DICT_FILES 100
#define BACKSPACE_CODE 127 // for ~ICANON
#define EOF_CODE 4 // for ~ICANON


typedef struct DictEntry {
	char *src;
	char *dest;
} DictEntry;

struct termios orig;

DictEntry *load_dict(char *path, int *dict_len_buf);
void translate_word(const char *word, DictEntry *dict,  int dict_len, char *res_buffer);
int compare_translations(const void *t1, const void *t2);
void enable_raw_mode();
void disable_raw_mode();
void replace_input_res_raw(int input_len, char *res);
int copy_buf_to_clipboard(const char *buffer, const char *command);
void free_dict(DictEntry *dict, int dict_len);
char *get_dict_path(char *dict_path_buf);

const char *get_copy_cmd();

/*
TODO:

+ EOF and '\n' behaviour
+ handle unsorted dictionary configs
+ delete during input
+ check for .dict file
	(current behaviour: looks for *.dict, prioritising alpha unless TRS_DEFAULT_DICT env var is set)
	- add --dict= flag?
	- define default dict thru env var?
- fix bugs on arrows and other control input
- refactor loading to dynamic realloc
- non-interactive mode

KNOWN BUGS:

- inconsistent behaviour on spaces in copy-paste buffer:
					currently removes leading spaces
*/
int main(int argc, const char *argv[]) {
	char input_buf[MAX_INPUT_LEN];
	char res_buf[MAX_RES_LEN];
	char clip_buf[CLIPBOARD_SIZE] = {0};
	int dict_len;
	char dict_path_buf[MAX_DICT_PATH_LEN];
	const char *copy_cmd = get_copy_cmd();

	DictEntry *dict = load_dict(get_dict_path(dict_path_buf), &dict_len);
	if (!dict) return -1;

	enable_raw_mode();

	int i = 0;
	while (1) {
		int c = getchar();

		// EOF or Enter - translate buffer, copy to clipboard and exit
		if (c == EOF_CODE || c == '\n') {
			input_buf[i] = '\0';
			int input_len = i;
			translate_word(input_buf, dict, dict_len, res_buf);
			replace_input_res_raw(input_len, res_buf);
			printf("\n");

			// append to clipboard buffer if compatible
			if (copy_cmd) {
				// not first word
				if (strlen(clip_buf) > 0)
					strncat(clip_buf, " ", CLIPBOARD_SIZE - strlen(clip_buf) - 1);
				strncat(clip_buf, res_buf, CLIPBOARD_SIZE - strlen(clip_buf) - 1);
				copy_buf_to_clipboard(clip_buf, copy_cmd);
			}
			break;
		}

       	// space - translate buffer and echo
		else if (c == ' ') {
        	input_buf[i] = '\0';
			int input_len = i;
			i = 0;
			translate_word(input_buf, dict, dict_len, res_buf);
			// just echo if ntng to translate
			if (input_len == 0 && strlen(res_buf) == 0)
				putchar(' ');
			// replace input with translation and echo
			else
				replace_input_res_raw(input_len, res_buf);
			fflush(stdout);

			// append to clipboard buffer if compatible
			if (copy_cmd) {
				int clip_buf_len = strlen(clip_buf);
				if (clip_buf_len > 0)
					strncat(clip_buf, " ", CLIPBOARD_SIZE - clip_buf_len - 1);
				strncat(clip_buf, res_buf, CLIPBOARD_SIZE - clip_buf_len - 1);
			}
		}

		// backspace - remove previous
		else if (c == BACKSPACE_CODE) {
			if (i > 0) {
				i--;
				printf("\b");
				printf(" ");
				printf("\b");
				fflush(stdout);
			}
		}

		// any other key - put in buffer and echo
		else if (i < MAX_INPUT_LEN - 1) {
        	input_buf[i++] = c;
        	putchar(c);
        	fflush(stdout);
        }
	}
	free_dict(dict, dict_len);
	return 0;
}

char *get_dict_path(char *dict_path_buf) {
	const char *home_path = getenv("HOME");
	if (!home_path) {perror("getenv"); return NULL;}

	// assuming $HOME/.config/trs/
	snprintf(dict_path_buf, MAX_DICT_PATH_LEN, "%s/.config/trs", home_path);

	DIR *dir = opendir(dict_path_buf);
	if (!dir) {perror("opendir"); return NULL;}
	struct dirent *entry;
	char *prior_file = NULL;

	while ((entry = readdir(dir))) {
		// look for *.dict
		if (strstr(entry->d_name, DICT_FILE_EXTENSION)) {
			// get 1st by name in alpanum order
			if (!prior_file || strcmp(entry->d_name, prior_file) < 0) {
				free(prior_file);
				prior_file = strdup(entry->d_name);
			}
		}
	}
	closedir(dir);

	if (!prior_file) {return NULL;}
	strncat(dict_path_buf, "/", MAX_DICT_PATH_LEN - strlen(dict_path_buf) - 1);
    strncat(dict_path_buf, prior_file, MAX_DICT_PATH_LEN - strlen(dict_path_buf) - 1);
    free(prior_file);
	return dict_path_buf;
}

// returns env name
const char *get_copy_cmd() {
	const char *session = getenv("XDG_SESSION_TYPE");
	if (!session) return NULL;

	// wayland
	if (!strcmp(session, XDG_WAYLAND_NAME)) {
		if (!system("command -v wl-copy > /dev/null"))
			return WAYLAND_COPY_CMD;
		else
			printf("wl-copy not found on PATH, copy feature unavaliable\n");
	}
	// x11
	else if (!strcmp(session, XDG_X11_NAME)) {
		if (!system("command -v xclip > /dev/null"))
			return X11_COPY_CMD;
		else
			printf("xclip not found on PATH, copy feature unavaliable\n");
	}
	// unsupported
	else
		printf("copy feature not supported on your environment\n");
	return NULL;
}

void free_dict(DictEntry *dict, int dict_len) {
	int i = 0;
	while (i < dict_len) {
		free(dict[i].dest);
		free(dict[i].src);
		i++;
	}
	free(dict);
}

int copy_buf_to_clipboard(const char *buffer, const char *command) {
	FILE *copy_proc = popen(command, "w");
	if (!copy_proc) {perror("popen"); return 0;}

	fputs(buffer, copy_proc);
	pclose(copy_proc);
	return 1;
}

void replace_input_res_raw(int input_len, char *res) {
	// override if anything written
	printf("\033[%dD", input_len);
	printf("%s ", res);
	printf("\033[K");
	fflush(stdout);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig);
    struct termios raw = orig;

    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    atexit(disable_raw_mode);
}

void disable_raw_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

DictEntry *load_dict(char *path, int *dict_len_buf) {
	FILE *dict_file = fopen(path, "r");
	if (!dict_file) {perror("fopen"); return NULL;}

	char dict_file_line_buffer[MAX_DICT_LINE_LEN];
	int line_count = 0;

	// count dict_len
	while (fgets(dict_file_line_buffer, sizeof(dict_file_line_buffer), dict_file)) {
		char *dest = strtok(dict_file_line_buffer, "=");
		char *src = strtok(NULL, "\n");
		// dont count bad parsing
		if (!dest || !src) continue;

		line_count++;
	}
	*dict_len_buf = line_count;

	rewind(dict_file);

	int i = 0;
	DictEntry *dict = malloc(sizeof(DictEntry) * line_count);
	if (!dict) {
		perror("malloc");
		fclose(dict_file);
		return NULL;
	}
	// parse DEST=SRC
	while (fgets(dict_file_line_buffer, sizeof(dict_file_line_buffer), dict_file)) {
		char *dest = strtok(dict_file_line_buffer, "=");
		char *src = strtok(NULL, "\n");
		// dont load bad parsing
		if (!dest || !src) continue;

		dict[i].dest = strdup(dest);
		dict[i].src = strdup(src);
		if (!dict[i].dest || !dict[i].src) {
			perror("strdup");
			free_dict(dict, *dict_len_buf);
			fclose(dict_file);
			return NULL;
		}
		++i;
	}

	fclose(dict_file);

	// sort
	qsort(dict, line_count, sizeof(DictEntry), compare_translations);
	return dict;
}

// sort by len of src in desc
int compare_translations(const void *t1, const void *t2) {
	const DictEntry *translation1 = t1;
	const DictEntry *translation2 = t2;

	size_t t1_src_len = strlen(translation1->src);
	size_t t2_src_len = strlen(translation2->src);

	return t1_src_len > t2_src_len ? -1 : t1_src_len < t2_src_len ? 1 : 0;

}


void translate_word(const char *word, DictEntry *dict,  int dict_len, char *res_buffer) {
	int i = 0, i_res = 0;
	int word_len = strlen(word);
	while (i < word_len) {
		int recognised = 0;
		for (int j = 0; j < dict_len; ++j) {
			const char *src_letter = dict[j].src;

			// compare strlen(src) letters with word starting from i
			// hence dict should be sorted by strlen(src) in desc
			if (!strncmp(src_letter, word + i, strlen(src_letter))) {
				recognised = 1;
				const char *dest_letter = dict[j].dest;
				// result buffer full
				if ((i_res + strlen(dest_letter) + 1) > MAX_RES_LEN) break;
				strcpy(res_buffer + i_res, dest_letter);
				i_res += strlen(dest_letter);

				i += strlen(src_letter);
				break;
			}
		}
		// just copy orig char
		if (!recognised)
			res_buffer[i_res++] = word[i++];
	}
	res_buffer[i_res] = '\0';
}