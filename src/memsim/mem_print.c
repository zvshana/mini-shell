/* Printing of the simulator state: RAM, swap file, page table and TLB. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memsim/mem_sim.h"

/* Prints one page as hex bytes followed by its printable characters. */
static void print_page_bytes(const char *bytes, int size)
{
    for (int i = 0; i < size; i++)
        printf("%02X ", (unsigned char)bytes[i]);

    printf("| ");
    for (int i = 0; i < size; i++) {
        char c = bytes[i];
        putchar(c >= 32 && c <= 126 ? c : '.');
    }
    printf("\n");
}

void print_memory(sim_database *sim)
{
    if (!sim || !sim->main_memory) {
        printf("Error: Invalid memory simulation structure\n");
        return;
    }

    printf("=== MAIN MEMORY CONTENTS ===\n");
    printf("Memory size: %d bytes, Page size: %d bytes, Number of frames: %d\n",
           sim->memory_size, sim->page_size, sim->num_frames);

    for (int frame = 0; frame < sim->num_frames; frame++) {
        printf("Frame %d: ", frame);
        print_page_bytes(sim->main_memory + frame * sim->page_size, sim->page_size);
    }
    printf("=============================\n\n");
}

void print_swap(sim_database *sim)
{
    if (!sim || sim->swapfile_fd < 0) {
        printf("Error: Invalid swap file\n");
        return;
    }

    printf("=== SWAP FILE CONTENTS ===\n");
    printf("Swap size: %d bytes, Page size: %d bytes, Number of swap pages: %d\n",
           sim->swap_size, sim->page_size, sim->num_swap_slots);

    char *buffer = malloc((size_t)sim->page_size);
    if (!buffer) {
        perror("Error allocating buffer for swap reading");
        return;
    }

    for (int slot = 0; slot < sim->num_swap_slots; slot++) {
        if (lseek(sim->swapfile_fd, (off_t)slot * sim->page_size, SEEK_SET) == -1) {
            perror("Error seeking in file");
            break;
        }
        if (read(sim->swapfile_fd, buffer, (size_t)sim->page_size) != sim->page_size) {
            printf("Swap Page %d: [Error reading]\n", slot);
            continue;
        }
        printf("Swap Page %d: ", slot);
        print_page_bytes(buffer, sim->page_size);
    }

    free(buffer);
    printf("===========================\n\n");
}

void print_page_table(sim_database *sim)
{
    if (!sim || !sim->page_table) {
        printf("Error: Invalid page table\n");
        return;
    }

    printf("=== PAGE TABLE ===\n");
    printf("Number of pages: %d\n", sim->num_pages);
    printf("Page | V | D | P | Frame/Swap | Segment\n");
    printf("-----|---|---|---|------------|--------\n");

    int ps = sim->page_size;
    int text_pages = (sim->text_size + ps - 1) / ps;
    int data_pages = (sim->data_size + ps - 1) / ps;
    int bss_pages  = (sim->bss_size + ps - 1) / ps;

    for (int page = 0; page < sim->num_pages; page++) {
        const page_descriptor *pd = &sim->page_table[page];
        const char *segment;

        if (page < text_pages)
            segment = "TEXT";
        else if (page < text_pages + data_pages)
            segment = "DATA";
        else if (page < text_pages + data_pages + bss_pages)
            segment = "BSS";
        else
            segment = "H/S";   /* heap / stack */

        printf("%4d | %d | %d | %d |", page, pd->V, pd->D, pd->P);
        if (pd->frame_swap == -1)
            printf("      -    |");
        else
            printf("    %4d   |", pd->frame_swap);
        printf(" %s\n", segment);
    }

    printf("==================\n");
    printf("Legend: V=Valid, D=Dirty, P=Permission (1=Read-Only, 0=Read/Write)\n");
    printf("        Frame/Swap: Frame number if in memory (V=1), Swap page if swapped out\n\n");
}

void print_tlb(sim_database *sim)
{
    if (!sim || !sim->tlb) {
        printf("TLB not implemented or invalid\n");
        return;
    }

    printf("=== TLB CONTENTS ===\n");
    printf("TLB size: %d entries\n", sim->tlb_size);
    printf("Entry | Valid | Page | Frame | Timestamp\n");
    printf("------|-------|------|-------|----------\n");

    for (int i = 0; i < sim->tlb_size; i++) {
        const tlb_entry *entry = &sim->tlb[i];
        printf("  %d   |   %d   |", i, entry->valid);
        if (entry->valid)
            printf(" %4d | %5d |  %8d\n", entry->page_number, entry->frame_number,
                   entry->timestamp);
        else
            printf("   -  |   -   |     -\n");
    }
    printf("====================\n\n");
}
