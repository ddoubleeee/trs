#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>

#define DICT_CFG_PATH "../config/test.dict"
#define XDG_X11_NAME "x11"
#define XDG_WAYLAND_NAME "wayland"
#define X11_COPY "xclip -selection clipboard"
#define WAYLAND_COPY "wl-copy"
#define MAX_DICT_CFG_LINE_LEN 1024
#define MAX_INPUT_LEN 1024
#define MAX_RES_LEN 1024
#define CLIPBOARD_SIZE 4096

#define BACKSPACE 127
// EOF for ~ICANON
#define CTRL_D 4


typedef struct Translation {
	char *src;
	char *dest;
} Translation;

struct termios orig;

Translation *load_dict(char *path, int *dict_len_buf);
int translate_word(const char *word, Translation *dict,  int dict_len, char *res_buffer);
void enable_raw_mode();
void disable_raw_mode();
void replace_input_res_raw(char *res, int input_len);
int copy_buf_to_clipboard(const char *buffer, const char *command);
int compare_translations(const void *t1, const void *t2);
const char *get_copy_cmd();

// void free_dict(Translation *dict);

/*
TODO:

+ EOF and '\n' behaviour
+ handle unsorted dictionary configs
- delete on raw input
- free on exit
- non-interactive mode

*/

int main(int argc, const char *argv[]) {
	const char *copy_cmd = get_copy_cmd();
	char input_buf[MAX_INPUT_LEN];
	char res_buf[MAX_RES_LEN];
	char clip_buf[CLIPBOARD_SIZE] = {0};
	int dict_len;
	Translation *dict = load_dict(DICT_CFG_PATH, &dict_len);
	enable_raw_mode();

	int i = 0;
	while (1) {
		int c = getchar();

		// EOF or Enter - translate buffer, copy to clipboard and exit
		if (c == CTRL_D || c == '\n') {
			input_buf[i] = '\0';
			int input_len = translate_word(input_buf, dict, dict_len, res_buf);
			if (copy_cmd) {
				// not first word
				if (strlen(clip_buf) > 0)
					strncat(clip_buf, " ", CLIPBOARD_SIZE - strlen(clip_buf) - 1);
				strncat(clip_buf, res_buf, CLIPBOARD_SIZE - strlen(clip_buf) - 1);
				copy_buf_to_clipboard(clip_buf, copy_cmd);
			}
			replace_input_res_raw(res_buf, input_len);
			printf("\n");
			break;
		}
       	// space - translate buffer
		else if (c == ' ') {
        	input_buf[i] = '\0';
			i = 0;

			int input_len = translate_word(input_buf, dict, dict_len, res_buf);
			// append to clipboard buffer if compatible
			if (copy_cmd) {
				// not first word
				if (strlen(clip_buf) > 0)
					strncat(clip_buf, " ", CLIPBOARD_SIZE - strlen(clip_buf) - 1);
				strncat(clip_buf, res_buf, CLIPBOARD_SIZE - strlen(clip_buf) - 1);
			}
			replace_input_res_raw(res_buf, input_len);
		}
		else if (i < MAX_INPUT_LEN - 1) {
		// any other key - put in buffer and echo
        	input_buf[i++] = c;
        	putchar(c);
        	fflush(stdout);
        }
        // overflow or garbage
		else {
        	/* free */
        	return -1;
        }
	}
	/* free */
	return 0;
}

// returns env name
const char *get_copy_cmd() {
	char *session = getenv("XDG_SESSION_TYPE");
	if (!session)
		return NULL;

	// wayland
	if (!strcmp(session, XDG_WAYLAND_NAME)) {
		if (!system("command -v wl-copy > /dev/null"))
			return WAYLAND_COPY;
		else
			printf("wl-copy not found on PATH, copy feature unavaliable\n");
	}
	// x11
	else if (!strcmp(session, XDG_X11_NAME)) {
		if (!system("command -v xclip > /dev/null"))
			return X11_COPY;
		else
			printf("xclip not found on PATH, copy feature unavaliable\n");
	}
	// unsupported
	else
		printf("copy feature not supported on your environment\n");

	return NULL;
}

int copy_buf_to_clipboard(const char *buffer, const char *command) {
	if (!buffer || !command)
		return 0;

	FILE *copy_proc = popen(command, "w");
	if (!copy_proc)
		return 0;

	fputs(buffer, copy_proc);
	pclose(copy_proc);
	return 1;
}

void replace_input_res_raw(char *res, int input_len) {
	// move input_len left
	printf("\033[%iD", input_len);
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
	if (dict_cfg_file) {
		char dict_cfg_line_buffer[MAX_DICT_CFG_LINE_LEN];
		int line_count = 0;

		while (fgets(dict_cfg_line_buffer, sizeof(dict_cfg_line_buffer), dict_cfg_file))
			line_count++;
		*dict_len_buf = line_count;

		rewind(dict_cfg_file);

		int i = 0;
		Translation *dict = malloc(sizeof(Translation) * line_count);
		while (fgets(dict_cfg_line_buffer, sizeof(dict_cfg_line_buffer), dict_cfg_file)) {

			char *dest = strtok(dict_cfg_line_buffer, "=");
			char *src = strtok(NULL, "\n");

			dict[i].dest = malloc(strlen(dest) + 1);
			dict[i].src = malloc(strlen(src) + 1);

			strcpy(dict[i].dest, dest);
			strcpy(dict[i].src, src);
			++i;
		}

		fclose(dict_cfg_file);

		// sort
		qsort(dict, line_count, sizeof(Translation), compare_translations);
		return dict;
	}
	perror("fopen");
	return NULL;
}

// sort by len of src in desc
int compare_translations(const void *trans1, const void *trans2) {
	const Translation *t1 = trans1;
	const Translation *t2 = trans2;

	size_t t1_src_len = strlen(t1->src);
	size_t t2_src_len = strlen(t2->src);

	return t1_src_len > t2_src_len ? -1 : t1_src_len < t2_src_len ? 1 : 0;

}


int translate_word(const char *word, Translation *dict,  int dict_len, char *res_buffer) {
	int i = 0, res_i = 0;
	int word_len = strlen(word);
	while (i < word_len) {
		int recognised = 0;
		for (int j = 0; j < dict_len; ++j) {
			const char *src_letter = dict[j].src;

			// compare n letters (len of src) with word starting from i
			// hence dict should be declared in desc priority order
			if (strncmp(src_letter, word + i, strlen(src_letter)) == 0) {
				recognised = 1;
				const char *dest_letter = dict[j].dest;

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

	return word_len;
}
