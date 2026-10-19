#include "memsim/mem_sim.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memsim/tlb.h"
#include "utils/str_utils.h"

#define EMPTY_BYTE      '-'
#define SWAP_FILL_CHUNK 512
#define CONFIG_LINE_MAX 1024

/* ------------------------------------------------------------------ */
/*  Configuration                                                     */
/* ------------------------------------------------------------------ */

static bool parse_size(const char *s, int *out)
{
    if (!str_is_unsigned(s))
        return false;

    errno = 0;
    long value = strtol(s, NULL, 10);
    if (errno != 0 || value > INT_MAX)
        return false;

    *out = (int)value;
    return true;
}

static bool is_power_of_two(int x)
{
    return x > 0 && (x & (x - 1)) == 0;
}

bool sim_parse_config(const char *line, sim_config *cfg)
{
    int count;
    char **words = str_split_words(line, &count);

    bool ok = count == SIM_CONFIG_FIELDS &&
              str_ends_with(words[0], ".bin") && strlen(words[0]) < SIM_MAX_PATH &&
              str_ends_with(words[1], ".bin") && strlen(words[1]) < SIM_MAX_PATH;

    int *sizes[] = {
        &cfg->text_size, &cfg->data_size, &cfg->bss_size, &cfg->heap_stack_size,
        &cfg->page_size, &cfg->num_pages, &cfg->memory_size, &cfg->swap_size,
    };
    for (int i = 0; ok && i < SIM_CONFIG_FIELDS - 2; i++)
        ok = parse_size(words[i + 2], sizes[i]);

    if (ok) {
        strcpy(cfg->program_name, words[0]);
        strcpy(cfg->swap_name, words[1]);

        long segments = (long)cfg->text_size + cfg->data_size +
                        cfg->bss_size + cfg->heap_stack_size;
        ok = is_power_of_two(cfg->page_size) &&
             cfg->num_pages > 0 &&
             cfg->memory_size >= cfg->page_size &&
             segments <= (long)cfg->num_pages * cfg->page_size;
    }

    str_free_words(words);
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Low-level file helpers                                            */
/* ------------------------------------------------------------------ */

static bool seek_to(int fd, off_t offset)
{
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking in file");
        return false;
    }
    return true;
}

static bool read_block(int fd, off_t offset, char *dst, int size)
{
    if (!seek_to(fd, offset))
        return false;

    ssize_t n = read(fd, dst, (size_t)size);
    if (n != size) {
        if (n >= 0)
            errno = EIO;  /* short read: file smaller than expected */
        perror("Error reading from file");
        return false;
    }
    return true;
}

static bool write_block(int fd, off_t offset, const char *src, int size)
{
    if (!seek_to(fd, offset))
        return false;

    if (write(fd, src, (size_t)size) != size) {
        perror("Error writing to swap file");
        return false;
    }
    return true;
}

/* Fills the whole swap file with EMPTY_BYTE. */
static bool fill_swap(const sim_database *sim)
{
    char chunk[SWAP_FILL_CHUNK];
    memset(chunk, EMPTY_BYTE, sizeof chunk);

    for (int left = sim->swap_size; left > 0;) {
        int n = left < (int)sizeof chunk ? left : (int)sizeof chunk;
        if (write(sim->swapfile_fd, chunk, (size_t)n) != n) {
            perror("Error writing to swap file");
            return false;
        }
        left -= n;
    }
    return true;
}

/* Joins the directory of `script_path` with a relative `name`. */
static void resolve_path(const char *script_path, const char *name, char *out, size_t size)
{
    const char *slash = strrchr(script_path, '/');
    if (name[0] == '/' || !slash)
        snprintf(out, size, "%s", name);
    else
        snprintf(out, size, "%.*s/%s", (int)(slash - script_path), script_path, name);
}

static bool read_first_line(const char *path, char *buf, size_t size)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open script file %s\n", path);
        return false;
    }

    bool ok = fgets(buf, (int)size, fp) != NULL;
    fclose(fp);
    if (ok)
        buf[strcspn(buf, "\r\n")] = '\0';
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Initialization / cleanup                                          */
/* ------------------------------------------------------------------ */

static void *alloc_array(int count, size_t size)
{
    return calloc(count > 0 ? (size_t)count : 1, size);
}

static int log2_int(int x)
{
    int bits = 0;
    while (x > 1) {
        x >>= 1;
        bits++;
    }
    return bits;
}

sim_database *init_system(const char *script_path)
{
    char line[CONFIG_LINE_MAX];
    sim_config cfg;

    if (!read_first_line(script_path, line, sizeof line))
        return NULL;
    if (!sim_parse_config(line, &cfg)) {
        fprintf(stderr, "Error: Invalid script format\n");
        return NULL;
    }

    sim_database *sim = calloc(1, sizeof *sim);
    if (!sim) {
        perror("malloc");
        return NULL;
    }
    sim->program_fd = -1;
    sim->swapfile_fd = -1;

    snprintf(sim->program_name, sizeof sim->program_name, "%s", cfg.program_name);
    sim->text_size       = cfg.text_size;
    sim->data_size       = cfg.data_size;
    sim->bss_size        = cfg.bss_size;
    sim->heap_stack_size = cfg.heap_stack_size;
    sim->page_size       = cfg.page_size;
    sim->num_pages       = cfg.num_pages;
    sim->memory_size     = cfg.memory_size;
    sim->swap_size       = cfg.swap_size;
    sim->num_frames      = cfg.memory_size / cfg.page_size;
    sim->num_swap_slots  = cfg.swap_size / cfg.page_size;
    sim->tlb_size        = TLB_SIZE;
    sim->offset_bits     = log2_int(cfg.page_size);

    char path[2 * SIM_MAX_PATH];

    resolve_path(script_path, cfg.program_name, path, sizeof path);
    sim->program_fd = open(path, O_RDONLY);
    if (sim->program_fd == -1) {
        perror("Error opening program file");
        clear_system(sim);
        return NULL;
    }

    resolve_path(script_path, cfg.swap_name, path, sizeof path);
    sim->swapfile_fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (sim->swapfile_fd == -1) {
        perror("Error creating/opening swap file");
        clear_system(sim);
        return NULL;
    }

    sim->page_table       = alloc_array(sim->num_pages, sizeof *sim->page_table);
    sim->main_memory      = alloc_array(sim->memory_size, sizeof *sim->main_memory);
    sim->tlb              = alloc_array(sim->tlb_size, sizeof *sim->tlb);
    sim->frame_used       = alloc_array(sim->num_frames, sizeof *sim->frame_used);
    sim->swap_used        = alloc_array(sim->num_swap_slots, sizeof *sim->swap_used);
    sim->page_last_access = alloc_array(sim->num_pages, sizeof *sim->page_last_access);
    if (!sim->page_table || !sim->main_memory || !sim->tlb || !sim->frame_used ||
        !sim->swap_used || !sim->page_last_access) {
        perror("malloc");
        clear_system(sim);
        return NULL;
    }

    int text_pages = sim->text_size / sim->page_size;
    for (int i = 0; i < sim->num_pages; i++) {
        sim->page_table[i] = (page_descriptor){
            .V = 0,
            .D = 0,
            .P = i < text_pages ? 1 : 0,
            .frame_swap = -1,
        };
    }
    memset(sim->main_memory, EMPTY_BYTE, (size_t)sim->memory_size);
    tlb_init(sim);

    if (!fill_swap(sim)) {
        clear_system(sim);
        return NULL;
    }
    return sim;
}

void clear_system(sim_database *sim)
{
    if (!sim)
        return;

    if (sim->program_fd != -1)
        close(sim->program_fd);
    if (sim->swapfile_fd != -1)
        close(sim->swapfile_fd);

    free(sim->page_table);
    free(sim->main_memory);
    free(sim->tlb);
    free(sim->frame_used);
    free(sim->swap_used);
    free(sim->page_last_access);
    free(sim);
}

/* ------------------------------------------------------------------ */
/*  Paging                                                            */
/* ------------------------------------------------------------------ */

static int address_space_size(const sim_database *sim)
{
    return sim->text_size + sim->data_size + sim->bss_size + sim->heap_stack_size;
}

static bool check_address(const sim_database *sim, int address)
{
    if (address < 0 || address >= address_space_size(sim)) {
        fprintf(stderr, "Error: Invalid address %d (out of range)\n", address);
        return false;
    }
    return true;
}

static int page_of(const sim_database *sim, int address)
{
    return address >> sim->offset_bits;
}

static int offset_of(const sim_database *sim, int address)
{
    return address & (sim->page_size - 1);
}

static bool is_data_page(const sim_database *sim, int page)
{
    int start = page * sim->page_size;
    return start >= sim->text_size && start < sim->text_size + sim->data_size;
}

static int find_free_frame(const sim_database *sim)
{
    for (int i = 0; i < sim->num_frames; i++) {
        if (!sim->frame_used[i])
            return i;
    }
    return -1;
}

/* First-fit search for a free swap slot. */
static int find_free_swap_slot(const sim_database *sim)
{
    for (int i = 0; i < sim->num_swap_slots; i++) {
        if (!sim->swap_used[i])
            return i;
    }
    return -1;
}

/* The page in RAM that was accessed least recently. */
static int find_lru_page(const sim_database *sim)
{
    int victim = -1;
    for (int i = 0; i < sim->num_pages; i++) {
        if (sim->page_table[i].V &&
            (victim == -1 || sim->page_last_access[i] < sim->page_last_access[victim]))
            victim = i;
    }
    return victim;
}

/* Frees a frame by evicting the LRU page. Dirty pages are written to swap;
 * clean pages are simply dropped (they can be reloaded from their source).
 * Returns the freed frame, or -1 on error. */
static int evict_page(sim_database *sim)
{
    int victim = find_lru_page(sim);
    if (victim == -1)
        return -1;

    page_descriptor *pd = &sim->page_table[victim];
    int frame = pd->frame_swap;
    int swap_slot = -1;

    if (pd->D) {
        printf("Page replacement: Evicting page %d to swap\n", victim);

        swap_slot = find_free_swap_slot(sim);
        if (swap_slot == -1) {
            fprintf(stderr, "Error: No free swap block available\n");
            return -1;
        }
        if (!write_block(sim->swapfile_fd, (off_t)swap_slot * sim->page_size,
                         sim->main_memory + frame * sim->page_size, sim->page_size))
            return -1;
        sim->swap_used[swap_slot] = true;
    }

    pd->V = 0;
    pd->frame_swap = swap_slot;
    tlb_invalidate(sim, victim);
    sim->frame_used[frame] = false;
    return frame;
}

/* Brings `page` into RAM (page fault handling). Returns its frame or -1. */
static int page_in(sim_database *sim, int page)
{
    int frame = find_free_frame(sim);
    if (frame == -1 && (frame = evict_page(sim)) == -1)
        return -1;

    page_descriptor *pd = &sim->page_table[page];
    char *dst = sim->main_memory + frame * sim->page_size;
    off_t program_offset = (off_t)page * sim->page_size;
    bool ok = true;

    if (pd->D) {
        printf("Page fault: Loading page %d from swap file\n", page);
        ok = read_block(sim->swapfile_fd, (off_t)pd->frame_swap * sim->page_size,
                        dst, sim->page_size);
        if (ok)
            sim->swap_used[pd->frame_swap] = false;
    } else if (pd->P) {
        printf("Page fault: Loading page %d from program file (text)\n", page);
        ok = read_block(sim->program_fd, program_offset, dst, sim->page_size);
    } else if (is_data_page(sim, page)) {
        printf("Page fault: Loading page %d from program file (data)\n", page);
        ok = read_block(sim->program_fd, program_offset, dst, sim->page_size);
    } else {
        /* bss / heap / stack: a brand new zero-filled page */
        memset(dst, 0, (size_t)sim->page_size);
    }

    if (!ok)
        return -1;

    sim->frame_used[frame] = true;
    pd->V = 1;
    pd->frame_swap = frame;
    tlb_update(sim, page, frame);
    return frame;
}

/* Translates `page` to a frame: TLB first, then the page table, then a
 * page fault. Returns the frame or -1. */
static int translate(sim_database *sim, int page)
{
    int frame = tlb_lookup(sim, page);
    if (frame != -1) {
        printf("TLB Hit: Page %d -> Frame %d\n", page, frame);
        return frame;
    }
    printf("TLB Miss: Page %d\n", page);

    page_descriptor *pd = &sim->page_table[page];
    if (!pd->V)
        return page_in(sim, page);

    tlb_update(sim, page, pd->frame_swap);
    return pd->frame_swap;
}

char load(sim_database *sim, int address)
{
    if (!check_address(sim, address))
        return '-';

    int page = page_of(sim, address);
    sim->page_last_access[page] = ++sim->lru_clock;

    int frame = translate(sim, page);
    if (frame == -1)
        return '-';
    return sim->main_memory[frame * sim->page_size + offset_of(sim, address)];
}

void store(sim_database *sim, int address, char value)
{
    if (!check_address(sim, address))
        return;

    int page = page_of(sim, address);
    if (sim->page_table[page].P) {
        fprintf(stderr, "Error: Invalid write operation to read-only segment at address %d\n",
                address);
        return;
    }

    sim->page_last_access[page] = ++sim->lru_clock;

    int frame = translate(sim, page);
    if (frame == -1)
        return;

    sim->main_memory[frame * sim->page_size + offset_of(sim, address)] = value;
    sim->page_table[page].D = 1;
    printf("Stored value '%c' at address %d\n", value, address);
}
