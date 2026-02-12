#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>

#define DICT_CFG_PATH "../config/test.dict"
#define XDG_X11_NAME "x11"
#define XDG_WAYLAND_NAME "wayland"
#define X11_COPY_CMD "xclip -selection clipboard"
#define WAYLAND_COPY_CMD "wl-copy"
#define MAX_DICT_CFG_LINE_LEN 1024
#define MAX_INPUT_LEN 1024
#define MAX_RES_LEN 1024
#define CLIPBOARD_SIZE 4096
#define BACKSPACE 127
#define DELETE
// EOF for ~ICANON
#define CTRL_D 4
typedef struct Translation {
	char *src;
	char *dest;
} Translation;

struct termios orig;

Translation *load_dict(char *path, int *dict_len_buf);
void translate_word(const char *word, Translation *dict,  int dict_len, char *res_buffer);
int compare_translations(const void *t1, const void *t2);
void enable_raw_mode();
void disable_raw_mode();
void replace_input_res_raw(char *res, int input_len);
int copy_buf_to_clipboard(const char *buffer, const char *command);
void free_dict(Translation *dict, int dict_len);

const char *get_copy_cmd();

/*
TODO:

+ EOF and '\n' behaviour
+ handle unsorted dictionary configs
+ delete during input
- fix bugs on arrows and other control input
- free on exit
- non-interactive mode

*/
int main(int argc, const char *argv[]) {
	char input_buf[MAX_INPUT_LEN];
	char res_buf[MAX_RES_LEN];
	char clip_buf[CLIPBOARD_SIZE] = {0};
	int dict_len;
	const char *copy_cmd = get_copy_cmd();

	Translation *dict = load_dict(DICT_CFG_PATH, &dict_len);
	if (!dict) return -1;

	enable_raw_mode();

	int i = 0;
	while (1) {
		int c = getchar();

		// EOF or Enter - translate buffer, copy to clipboard and exit
		if (c == CTRL_D || c == '\n') {
			input_buf[i] = '\0';
			int input_len = i;
			translate_word(input_buf, dict, dict_len, res_buf);
			replace_input_res_raw(res_buf, input_len);
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

       	// space - translate buffer
		else if (c == ' ') {
        	input_buf[i] = '\0';
			int input_len = i;
			i = 0;
			translate_word(input_buf, dict, dict_len, res_buf);
			replace_input_res_raw(res_buf, input_len);

			// append to clipboard buffer if compatible
			if (copy_cmd) {
				if (strlen(clip_buf) > 0)
					strncat(clip_buf, " ", CLIPBOARD_SIZE - strlen(clip_buf) - 1);
				strncat(clip_buf, res_buf, CLIPBOARD_SIZE - strlen(clip_buf) - 1);
			}
		}

		// backspace - remove previous
		else if (c == BACKSPACE) {
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

// returns env name
const char *get_copy_cmd() {
	char *session = getenv("XDG_SESSION_TYPE");
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

void free_dict(Translation *dict, int dict_len) {
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

void replace_input_res_raw(char *res, int input_len) {
	// move input_len left
	printf("\033[%dD", input_len);
	// printing will overwrite
	printf("%s ", res);
	// erase from current position
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

Translation *load_dict(char *path, int *dict_len_buf) {
	FILE *dict_cfg_file = fopen(path, "r");
	if (!dict_cfg_file) {perror("fopen"); return NULL;}

	char dict_cfg_line_buffer[MAX_DICT_CFG_LINE_LEN];
	int line_count = 0;

	// count dict_len
	while (fgets(dict_cfg_line_buffer, sizeof(dict_cfg_line_buffer), dict_cfg_file))
		line_count++;
	*dict_len_buf = line_count;

	rewind(dict_cfg_file);

	int i = 0;
	Translation *dict = malloc(sizeof(Translation) * line_count);
	if (!dict) {perror("malloc"); return NULL;}
	// parse DEST=SRC
	while (fgets(dict_cfg_line_buffer, sizeof(dict_cfg_line_buffer), dict_cfg_file)) {
		char *dest = strtok(dict_cfg_line_buffer, "=");
		char *src = strtok(NULL, "\n");

		dict[i].dest = malloc(strlen(dest) + 1);
		dict[i].src = malloc(strlen(src) + 1);
		if (!dict[i].dest || !dict[i].src) {
			perror("malloc");
			free_dict(dict, *dict_len_buf);
			return NULL;
		}

		// store
		strcpy(dict[i].dest, dest);
		strcpy(dict[i].src, src);
		++i;
	}

	fclose(dict_cfg_file);

	// sort
	qsort(dict, line_count, sizeof(Translation), compare_translations);
	return dict;
}

// sort by len of src in desc
int compare_translations(const void *t1, const void *t2) {
	const Translation *translation1 = t1;
	const Translation *translation2 = t2;

	size_t t1_src_len = strlen(translation1->src);
	size_t t2_src_len = strlen(translation2->src);

	return t1_src_len > t2_src_len ? -1 : t1_src_len < t2_src_len ? 1 : 0;

}


void translate_word(const char *word, Translation *dict,  int dict_len, char *res_buffer) {
	int i = 0, res_i = 0;
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
				if ((res_i + strlen(dest_letter) + 1) > MAX_RES_LEN)
					break;
				strcpy(res_buffer + res_i, dest_letter);
				res_i += strlen(dest_letter);

				i += strlen(src_letter);
				break;
			}
		}
		// just copy orig char
		if (!recognised)
			res_buffer[res_i++] = word[i++];
	}
	res_buffer[res_i] = '\0';
}