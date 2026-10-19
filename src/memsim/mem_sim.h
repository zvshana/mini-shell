#ifndef MEMSIM_MEM_SIM_H
#define MEMSIM_MEM_SIM_H

#include <stdbool.h>

/*
 * Virtual memory simulator for a single process (demand paging).
 *
 * The virtual address space is split into segments, in this order:
 *     text (read-only) | data | bss | heap + stack
 *
 * Pages are loaded into RAM only on first access:
 *   - text and unmodified data pages are read from the program file,
 *   - unmodified bss / heap / stack pages start zero-filled,
 *   - dirty pages that were evicted are read back from the swap file.
 *
 * Policies: LRU for page replacement, first-fit for swap slots, and a
 * small TLB with LRU replacement.
 */

#define SIM_MAX_PATH      256
#define SIM_CONFIG_FIELDS 10
#define TLB_SIZE          4

typedef struct {
    int V;            /* valid: the page is in RAM */
    int D;            /* dirty: written since it was loaded */
    int P;            /* permission: 1 = read-only (text), 0 = read/write */
    int frame_swap;   /* frame number if V=1, swap slot if swapped out, else -1 */
} page_descriptor;

typedef struct {
    int page_number;
    int frame_number;
    int valid;
    int timestamp;    /* last use, for LRU replacement */
} tlb_entry;

/* First line of a vmem script:
 *   program.bin swap.bin text data bss heap_stack page_size num_pages mem_size swap_size */
typedef struct {
    char program_name[SIM_MAX_PATH];
    char swap_name[SIM_MAX_PATH];
    int  text_size;
    int  data_size;
    int  bss_size;
    int  heap_stack_size;
    int  page_size;
    int  num_pages;
    int  memory_size;
    int  swap_size;
} sim_config;

typedef struct sim_database {
    page_descriptor *page_table;
    int   swapfile_fd;
    int   program_fd;
    char *main_memory;
    int   text_size;
    int   data_size;
    int   bss_size;
    int   heap_stack_size;

    tlb_entry *tlb;

    int page_size;
    int num_pages;
    int memory_size;
    int swap_size;
    int num_frames;
    int tlb_size;

    /* Bookkeeping */
    char  program_name[SIM_MAX_PATH];
    int   offset_bits;        /* log2(page_size) */
    int   num_swap_slots;
    bool *frame_used;         /* per RAM frame */
    bool *swap_used;          /* per swap slot */
    int  *page_last_access;   /* per page, LRU clock value of the last access */
    int   lru_clock;
    int   tlb_clock;
} sim_database;

/* Parses and checks the configuration line. Returns false if invalid. */
bool sim_parse_config(const char *line, sim_config *cfg);

/*
 * Reads the configuration from the first line of `script_path`, opens the
 * program file, creates the swap file and allocates all tables.
 * Relative file names are resolved against the script's directory.
 * Returns NULL (after printing an error) on failure.
 */
sim_database *init_system(const char *script_path);

/* Returns the byte at `address`, or '-' on error. */
char load(sim_database *sim, int address);

/* Writes `value` at `address`. Prints an error and does nothing on failure. */
void store(sim_database *sim, int address, char value);

void print_memory(sim_database *sim);
void print_swap(sim_database *sim);
void print_page_table(sim_database *sim);
void print_tlb(sim_database *sim);

/* Closes the files and frees everything. Accepts NULL. */
void clear_system(sim_database *sim);

#endif /* MEMSIM_MEM_SIM_H */
