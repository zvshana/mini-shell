#include "memsim/tlb.h"

#include <stdio.h>

void tlb_init(sim_database *sim)
{
    for (int i = 0; i < sim->tlb_size; i++) {
        sim->tlb[i] = (tlb_entry){
            .page_number  = -1,
            .frame_number = -1,
            .valid        = 0,
            .timestamp    = 0,
        };
    }
    sim->tlb_clock = 0;
}

int tlb_lookup(sim_database *sim, int page)
{
    for (int i = 0; i < sim->tlb_size; i++) {
        tlb_entry *entry = &sim->tlb[i];
        if (entry->valid && entry->page_number == page) {
            entry->timestamp = ++sim->tlb_clock;
            return entry->frame_number;
        }
    }
    return -1;
}

void tlb_update(sim_database *sim, int page, int frame)
{
    /* Prefer an empty entry, otherwise the least recently used one. */
    int slot = 0;
    for (int i = 0; i < sim->tlb_size; i++) {
        if (!sim->tlb[i].valid) {
            slot = i;
            break;
        }
        if (sim->tlb[i].timestamp < sim->tlb[slot].timestamp)
            slot = i;
    }

    sim->tlb[slot] = (tlb_entry){
        .page_number  = page,
        .frame_number = frame,
        .valid        = 1,
        .timestamp    = ++sim->tlb_clock,
    };
    printf("TLB Updated: Page %d -> Frame %d\n", page, frame);
}

void tlb_invalidate(sim_database *sim, int page)
{
    for (int i = 0; i < sim->tlb_size; i++) {
        if (sim->tlb[i].valid && sim->tlb[i].page_number == page)
            sim->tlb[i].valid = 0;
    }
}
