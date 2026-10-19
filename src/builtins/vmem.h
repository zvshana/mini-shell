#ifndef BUILTINS_VMEM_H
#define BUILTINS_VMEM_H

/*
 * The `vmem` built-in: runs a virtual-memory simulation script.
 *
 * Script format:
 *   line 1:  program.bin swap.bin text data bss heap_stack page_size num_pages mem_size swap_size
 *   then:    load <address>
 *            store <address> <char>
 *            print table | ram | swap | tlb
 */
void vmem_run(const char *script_path);

#endif /* BUILTINS_VMEM_H */
