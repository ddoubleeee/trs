#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>

#define CFG_PATH "../config/test"
#define MAX_CFG_LINE_LEN 1024
#define MAX_INPUT_LEN 1024
#define MAX_RES_LEN 1024

#define BACKSPACE 127
// EOF for ~ICANON
#define CTRL_D 4


typedef struct Translation {
	char *src;
	char *dest;
} Translation;

struct termios orig;

Translation *load_dict(char *path, int *line_count_buf);
int translate_word(const char *word, Translation *dict,  int dict_len, char *res_buffer);
void enable_raw_mode();
void disable_raw_mode();
void replace_input_res_raw(char *res, int input_len);
// void free_dict(Translation *dict);

/*
TODO:

- implement EOF and '\n' behaviour
- handle unsorted configs
- implement non-interactive mode
- free on exit

*/

int main(int argc, const char *argv[]) {
	char input_buf[MAX_INPUT_LEN];
	char res_buf[MAX_RES_LEN];
	int dict_len;
	Translation *dict = load_dict(CFG_PATH, &dict_len);
	enable_raw_mode();

	int i = 0;
	while (1) {
		int c = getchar();

		// EOF or Enter - translate buffer, copy to clipboard and exit
		if (c == CTRL_D || c == '\n') {
			input_buf[i] = '\0';
			int input_len = translate_word(input_buf, dict, dict_len, res_buf);
			replace_input_res_raw(res_buf, input_len);
       	}
       	// space - translate buffer
        if (c == ' ') {
        	input_buf[i] = '\0';
			i = 0;
			int input_len = translate_word(input_buf, dict, dict_len, res_buf);
			replace_input_res_raw(res_buf, input_len);
		}
		// any other key - put in buffer and echo
        else if (i < MAX_INPUT_LEN - 1) {
        	input_buf[i++] = c;
        	putchar(c);
        	fflush(stdout);
        }
        // overflow or garbage
        else {
        	// free
        	return -1;
        }
	}
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

void free_dict(Translation *dict) {

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

Translation *load_dict(char *path, int *line_count_buf) {
	FILE *cfg_file = fopen(path, "r");
	if (cfg_file) {
		char cfg_line_buffer[MAX_CFG_LINE_LEN];
		int line_count = 0;

		while (fgets(cfg_line_buffer, sizeof(cfg_line_buffer), cfg_file))
			line_count++;
		*line_count_buf = line_count;

		rewind(cfg_file);

		int i = 0;
		Translation *dict = malloc(sizeof(Translation) * line_count);
		while (fgets(cfg_line_buffer, sizeof(cfg_line_buffer), cfg_file)) {
			char *dest = strtok(cfg_line_buffer, "=");
			char *src = strtok(NULL, "\n");

			dict[i].dest = malloc(strlen(dest) + 1);
			dict[i].src = malloc(strlen(src) + 1);

			strcpy(dict[i].dest, dest);
			strcpy(dict[i].src, src);
			++i;
		}

		return dict;
	}
	perror("fopen");
	return NULL;
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
