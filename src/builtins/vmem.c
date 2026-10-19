#include "builtins/vmem.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memsim/mem_sim.h"
#include "utils/str_utils.h"

#define VMEM_MAX_LINE 256

typedef enum { LINE_OK, LINE_EOF, LINE_TOO_LONG } LineStatus;

/* Reads one trimmed line from the script. */
static LineStatus read_line(FILE *fp, char *buf, size_t size)
{
    if (!fgets(buf, (int)size, fp))
        return LINE_EOF;

    size_t len = strcspn(buf, "\n");
    if (buf[len] != '\n' && !feof(fp))
        return LINE_TOO_LONG;

    buf[len] = '\0';
    str_trim(buf);
    return LINE_OK;
}

static bool is_print_target(const char *target)
{
    return strcmp(target, "table") == 0 || strcmp(target, "ram") == 0 ||
           strcmp(target, "swap") == 0 || strcmp(target, "tlb") == 0;
}

static bool is_valid_command(const char *line)
{
    int count;
    char **words = str_split_words(line, &count);
    bool ok = false;

    if (count == 2 && strcmp(words[0], "load") == 0)
        ok = str_is_unsigned(words[1]);
    else if (count == 3 && strcmp(words[0], "store") == 0)
        ok = str_is_unsigned(words[1]) && strlen(words[2]) == 1;
    else if (count == 2 && strcmp(words[0], "print") == 0)
        ok = is_print_target(words[1]);

    str_free_words(words);
    return ok;
}

/* Checks the whole script before anything is executed. */
static bool is_valid_script(FILE *fp)
{
    char line[VMEM_MAX_LINE];
    sim_config cfg;

    if (read_line(fp, line, sizeof line) != LINE_OK || !sim_parse_config(line, &cfg))
        return false;

    LineStatus status;
    while ((status = read_line(fp, line, sizeof line)) == LINE_OK) {
        if (!str_is_blank(line) && !is_valid_command(line))
            return false;
    }
    return status == LINE_EOF;
}

static int parse_address(const char *s)
{
    errno = 0;
    long value = strtol(s, NULL, 10);
    return (errno != 0 || value > INT_MAX) ? INT_MAX : (int)value;
}

static void run_command(sim_database *sim, const char *line)
{
    int count;
    char **words = str_split_words(line, &count);

    if (strcmp(words[0], "load") == 0) {
        int address = parse_address(words[1]);
        char value = load(sim, address);
        if (value != '-')
            printf("Value at address %d = %c\n", address, value);
    } else if (strcmp(words[0], "store") == 0) {
        store(sim, parse_address(words[1]), words[2][0]);
    } else if (strcmp(words[1], "table") == 0) {
        print_page_table(sim);
    } else if (strcmp(words[1], "ram") == 0) {
        print_memory(sim);
    } else if (strcmp(words[1], "swap") == 0) {
        print_swap(sim);
    } else {
        print_tlb(sim);
    }

    fflush(stdout);
    str_free_words(words);
}

void vmem_run(const char *script_path)
{
    FILE *fp = fopen(script_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open script file %s\n", script_path);
        return;
    }

    if (!is_valid_script(fp)) {
        fprintf(stderr, "Error: Invalid script format\n");
        fclose(fp);
        return;
    }

    sim_database *sim = init_system(script_path);
    if (!sim) {
        fclose(fp);
        return;
    }

    printf("Loaded program \"%s\" with text=%d, data=%d, bss=%d, heap_stack=%d.\n",
           sim->program_name, sim->text_size, sim->data_size, sim->bss_size,
           sim->heap_stack_size);

    char line[VMEM_MAX_LINE];
    rewind(fp);
    read_line(fp, line, sizeof line);   /* skip the configuration line */
    while (read_line(fp, line, sizeof line) == LINE_OK) {
        if (!str_is_blank(line))
            run_command(sim, line);
    }

    clear_system(sim);
    fclose(fp);
}
