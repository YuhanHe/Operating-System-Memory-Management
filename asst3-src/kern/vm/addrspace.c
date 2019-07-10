/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */


//creation of per-process address space
struct addrspace *
as_create(void)
{
    int i;
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize address space as needed.
	 */
	as->page_table = (paddr_t **)alloc_kpages(1);
	if (!as->page_table) {
	    kfree(as);
        as = NULL;
	    return NULL;
    }

	
	for (i = 0; i < PAGETABLE_SIZE; i++)
	    as->page_table[i] = NULL; // zero fill the first page

	as->regions = NULL;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

    
    /* regions copy routine */
    struct region * region_instance;
    struct region * regions_new = newas->regions;
    struct region * regions_old = old->regions;

    while (regions_old) {
        region_instance = kmalloc(sizeof(struct region));
        if (!region_instance) {
            as_destroy(newas);
            return ENOMEM; /* Out of memory */
        }

        region_instance->is_writeable = regions_old->is_writeable;
        region_instance->is_writeable_prev = regions_old->is_writeable_prev;
        region_instance->vbase = regions_old->vbase;
        region_instance->npages = regions_old->npages;
        
		region_instance->next = NULL;

        if (regions_new)
            regions_new->next = region_instance;
        else
            newas->regions = region_instance;

        regions_old = regions_old->next;
        regions_new = region_instance;
    }

    /* page table copy routine */
    int dirty_bit;
	int i, j;
    
    vaddr_t new_frame_addr;
    for (i = 0; i < PAGETABLE_SIZE; i++) {
        if (old->page_table[i]) {
            newas->page_table[i] = kmalloc(sizeof(paddr_t)*PAGETABLE_SIZE);
            if (!newas->page_table[i]) {
                as_destroy(newas);
                return ENOMEM; /* Out of memory */
            }

            for (j = 0; j < PAGETABLE_SIZE; j++) {
                if (old->page_table[i][j]) {
                    dirty_bit = old->page_table[i][j] & TLBLO_DIRTY;    
                    new_frame_addr = alloc_kpages(1);
                    memmove((void *)new_frame_addr, (const void *)PADDR_TO_KVADDR(old->page_table[i][j] & PAGE_FRAME), PAGE_SIZE);
                    newas->page_table[i][j] = (KVADDR_TO_PADDR(new_frame_addr) & PAGE_FRAME) | dirty_bit | TLBLO_VALID;
                } else {
                    newas->page_table[i][j] = 0;
                }
            }
        }
    }	
	
	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{

    int i, j;
    for (i = 0; i < PAGETABLE_SIZE; i++) {
        if (as->page_table[i]) {
            for (j = 0; j < PAGETABLE_SIZE; j++) {
                if (as->page_table[i][j])
                    free_kpages(PADDR_TO_KVADDR(as->page_table[i][j] & PAGE_FRAME));
            }
            kfree(as->page_table[i]);
            as->page_table[i] = NULL;
        } 
    }
    kfree(as->page_table);
    as->page_table = NULL;

    // free allocated regions
    struct region * regions, * regions_p;
    regions = regions_p = as->regions;
    while (regions) {
        regions = regions->next;
        kfree(regions_p);
        regions_p = regions;
    }
    if (regions_p) {
        kfree(regions_p);
        regions_p = NULL;
    }

    /* Free the struct address space itself. */
	if (as)
        kfree(as);
    as = NULL;
}

/*
 * as_activate() and as_deactivate() are copied from dumbvm.c
 */
void
as_activate(void)
{
	int i, spl;
	struct addrspace * addr_space;

	addr_space = proc_getas();
	if (!addr_space)
		return;

	/* Flush TLB by writing invalid data to the TLB. */
	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */
    size_t npages;
	struct region * region_instance;
    memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
    vaddr &= PAGE_FRAME;

    memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

    npages = memsize / PAGE_SIZE;
    
    region_instance = kmalloc(sizeof(struct region));
    if (!region_instance)
        return ENOMEM; /* Out of memory */
    region_instance->is_writeable = writeable;
    region_instance->is_writeable_prev = region_instance->is_writeable;
    region_instance->npages = npages;
    region_instance->vbase = vaddr;
    
	
    region_instance->next = NULL;
    
 
    if (!as->regions) {
        as->regions = region_instance;
    } else {
        struct region * regions, * regions_p;
        regions = regions_p = as->regions;
        while (regions != NULL && regions->vbase < region_instance->vbase) {
            regions_p = regions;
            regions = regions->next;
        }
        regions_p->next = region_instance;
        region_instance->next = regions;
    }
    
    (void) readable;
    (void) executable;
    return 0; 
}

int
as_prepare_load(struct addrspace *as)
{
    struct region * regions = as->regions;
    while (regions) {
        regions->is_writeable = 1;
        regions = regions->next;
    }
    return 0;
}


int
as_complete_load(struct addrspace *as)
{
    struct region * regions;
	regions = as->regions;
    while (regions) {
        regions->is_writeable = regions->is_writeable_prev;
        regions = regions->next;
    }

    int spl, i;
    spl = splhigh(); // disable interrupts
	for (i=0; i<NUM_TLB; i++)
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    splx(spl); // enable interrupts
    return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
    int err_no;
    err_no = as_define_region(as, USERSTACK - USERSTACKSIZE, USERSTACKSIZE, 1, 1, 1);
    if (err_no)
        return err_no;
    
    /* Initial user-level stack pointer. */
    *stackptr = USERSTACK;

	return 0;
}

