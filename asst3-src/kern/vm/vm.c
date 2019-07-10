#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <current.h>
#include <proc.h>
#include <copyinout.h>
#include <spl.h>

/*  Page table functions */

/* allocate root page table entry */ 
int vm_alloc_1st_pte(paddr_t ** page_table, uint32_t index) {
    int i;
    page_table[index] = kmalloc(sizeof(paddr_t)*PAGETABLE_SIZE);
    if (!page_table[index])
        return ENOMEM; /* Out of memory */

    memset(page_table[index], 0, sizeof(paddr_t)*PAGETABLE_SIZE);

    for (i = 0; i < PAGETABLE_SIZE; i++) 
        page_table[index][i] = 0;

    return 0;
}

/* allocate second level page table entry */
int vm_alloc_2nd_pte(paddr_t ** page_table, uint32_t first_index, uint32_t second_index, uint32_t dirty) {
    
    vaddr_t page_alloc = alloc_kpages(1);
    if (!page_alloc)
        return ENOMEM; /* Out of memory */
    
    paddr_t phys_page_alloc = KVADDR_TO_PADDR(page_alloc);
    
    /* Zero-fill the frame to be allocated */
    bzero((void *)PADDR_TO_KVADDR(phys_page_alloc), PAGE_SIZE);
        
    
    page_table[first_index][second_index] = (phys_page_alloc & PAGE_FRAME) | dirty | TLBLO_VALID;
    return 0;
}

void 
vm_bootstrap(void)
{
    /* 
     * Initialise VM sub-system.  
     * You probably want to initialise your frame table here as well.
     */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    int result, spl;
    bool is_new_2_alloc = false;
    paddr_t page_addr;   
    uint32_t dirty, entry_lo, entry_hi, first_index, second_index;
    struct addrspace * addr_space = NULL;
    paddr_t ** page_table = NULL;
    struct region * regions = NULL;
    
    switch (faulttype) {
	    case VM_FAULT_READONLY: // fault type read only?
            return EFAULT; /* Bad memory reference */

	    case VM_FAULT_READ:
            break;

	    case VM_FAULT_WRITE:
		    break;

	    default:
		    return EINVAL; /* Invalid argument */
	}
    page_addr = KVADDR_TO_PADDR(faultaddress);
    /* First level page table index */
    first_index = page_addr >> 22;
    /* Second level page table index */
    second_index = page_addr << 10 >> 22;

    addr_space = proc_getas();
    if (!addr_space)
        return EFAULT;

    page_table = addr_space->page_table;
    if (!page_table)
        return EFAULT;

    if (!curproc)
        return EFAULT; /* Bad memory reference */
    

    
    /* Allocate a new Second level page_table if the root entry is still NULL. */
    if (!page_table[first_index]) {
        result = vm_alloc_1st_pte(page_table, first_index);
        if (result)
            return result;
        
        is_new_2_alloc = true;
    }
    
    /* Allocate a new Second level page_table entry in case we can't find the entry. */
    if (!page_table[first_index][second_index]) {      
        regions = addr_space->regions;
  
        /* If page_table entry doesn't exist, check if the address is a valid virtual address inside a region */
        while (regions != NULL) {
            if (faultaddress >= regions->vbase && faultaddress < (regions->vbase + (regions->npages*PAGE_SIZE))) {

 			    /* Set the dirty bit if the region is writeable. */
 			    if (regions->is_writeable)
			    	dirty = TLBLO_DIRTY;
			    else
				    dirty = 0;

                break;
            }
            regions = regions->next;
        }
        
        /* If page fault address is not in any region, return error code. */
		if (regions == NULL) {
            if (is_new_2_alloc && page_table[first_index]) {
                kfree(page_table[first_index]);
                page_table[first_index] = NULL;
            }
            return EFAULT; /* Bad memory reference */
        }

        if (regions->is_writeable)
			dirty = TLBLO_DIRTY;
		else
			dirty = 0;
               
        result = vm_alloc_2nd_pte(page_table, first_index, second_index, dirty);
        if (result) {
            if (is_new_2_alloc && page_table[first_index]) {
                kfree(page_table[first_index]);
                page_table[first_index] = NULL;
            }
            return result;
        }
    }

    /* Entry low is physical frame, dirty bit, and valid bit. */
    entry_lo = page_table[first_index][second_index];
    
    /* Entry high is the virtual address and ASID (not implemented). */
    entry_hi = PAGE_FRAME & faultaddress;

	/* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();// disable interrupt
    
    /* Randomly add page_table entry to TLB. */
    tlb_random(entry_hi, entry_lo);
    
    splx(spl);      // undisable interrupt

    return 0;
}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}

