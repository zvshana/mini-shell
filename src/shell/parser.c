#include "shell/parser.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "utils/str_utils.h"

/* Words allowed in one segment: command name + MAX_ARGS + the operator. */
#define MAX_SEGMENT_WORDS (MAX_ARGS + 2)
/* Words allowed in the last segment: command name + MAX_ARGS. */
#define MAX_LAST_SEGMENT_WORDS (MAX_ARGS + 1)

static bool only_spaces_after(const char *line, size_t i)
{
    for (i++; line[i]; i++) {
        if (line[i] != ' ')
            return false;
    }
    return true;
}

void parse_input(const char *line, InputInfo *info)
{
    memset(info, 0, sizeof *info);

    size_t len = strlen(line);
    int total_words = str_count_words(line);
    int words_seen = 0;
    int segment_words = 0;  /* words since the last operator */
    bool in_word = false;

    for (size_t i = 0; i < len; i++) {
        char c = line[i];

        if (c == ' ') {
            in_word = false;
            if (words_seen > 0 && words_seen != total_words &&
                i + 1 < len && line[i + 1] == ' ')
                info->extra_spaces = true;
            continue;
        }

        if (!in_word) {
            words_seen++;
            segment_words++;
            in_word = true;
        }

        bool space_before = i > 0 && line[i - 1] == ' ';
        bool is_last_word = words_seen == total_words;

        if (c == '|') {
            if (!is_last_word && space_before && line[i + 1] == ' ') {
                info->pipe_index = (int)i;
                if (segment_words > MAX_SEGMENT_WORDS)
                    info->too_many_args = true;
                segment_words = 0;
            } else {
                info->pipe_index = -1;
            }
        } else if (c == '&') {
            info->background_index = only_spaces_after(line, i) ? (int)i : -1;
            if (segment_words > MAX_SEGMENT_WORDS)
                info->too_many_args = true;
            segment_words = 0;
        } else if (c == '2' && line[i + 1] == '>') {
            if (!is_last_word && space_before && line[i + 2] == ' ') {
                info->redirect_index = (int)i;
                if (segment_words > MAX_SEGMENT_WORDS)
                    info->too_many_args = true;
                segment_words = 0;
            } else {
                info->redirect_index = -1;
            }
            i++; /* skip '>' */
        }
    }

    info->word_count = total_words;
    if (segment_words > MAX_LAST_SEGMENT_WORDS)
        info->too_many_args = true;
}

bool validate_input(const InputInfo *info)
{
    bool valid = true;

    if (info->too_many_args) {
        printf("ERR_ARGS\n");
        valid = false;
    }
    if (info->extra_spaces) {
        printf("ERR_SPACE\n");
        valid = false;
    }
    if (info->pipe_index == -1 || info->background_index == -1 ||
        info->redirect_index == -1) {
        printf("ERR\n");
        valid = false;
    }
    return valid;
}
