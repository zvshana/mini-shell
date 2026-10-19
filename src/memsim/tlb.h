#ifndef MEMSIM_TLB_H
#define MEMSIM_TLB_H

#include "memsim/mem_sim.h"

/* Translation Lookaside Buffer with LRU replacement. */

void tlb_init(sim_database *sim);

/* Returns the frame of `page`, or -1 on a miss. A hit refreshes the entry. */
int tlb_lookup(sim_database *sim, int page);

/* Inserts page -> frame, replacing an invalid or least recently used entry. */
void tlb_update(sim_database *sim, int page, int frame);

/* Drops `page` from the TLB (called when the page leaves RAM). */
void tlb_invalidate(sim_database *sim, int page);

#endif /* MEMSIM_TLB_H */
