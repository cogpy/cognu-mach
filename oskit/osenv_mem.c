/*
 * Copyright (c) 1999, 2000 University of Utah and the Flux Group.
 * All rights reserved.
 *
 * This file is part of the Flux OSKit.  The OSKit is free software, also known
 * as "open source;" you can redistribute it and/or modify it under the terms
 * of the GNU General Public License (GPL), version 2, as published by the Free
 * Software Foundation (FSF).  To explore alternate licensing terms, contact
 * the University of Utah at csl-dist@cs.utah.edu or +1-801-585-3271.
 *
 * The OSKit is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GPL for more details.  You should have
 * received a copy of the GPL along with the OSKit; see the file COPYING.  If
 * not, write to the FSF, 59 Temple Place #330, Boston, MA 02111-1307, USA.
 */

#include <oskit/com.h>
#include <oskit/com/mem.h>
#include <oskit/com/services.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/osenv_mem.h>

#include <oskit/lmm.h>
#include <oskit/machine/base_vm.h>
#include <oskit/machine/physmem.h>
#include <oskit/machine/phys_lmm.h>

#include <cpus.h>
#include "lock.h"
#include "kalloc.h"
#include "zalloc.h"
#include "assert.h"
#include <machine/spl.h>

#include <vm/vm_kern.h>

#include "ds_oskit.h"

#define in_oskit_interrupt	(nested_pic_mask != 0)
extern unsigned int nested_pic_mask; /* osenv_irq.c */

decl_simple_lock_data(, phys_lmm_lock)
decl_simple_lock_data(extern, vm_page_queue_free_lock)

extern int vm_page_unqueued_count; /* vm_resident.c */
extern zone_t vm_page_zone;
extern vm_offset_t virtual_space_start;
extern vm_offset_t virtual_space_end;

int lmm_wants_pages;

#define debug_protect_page(p)	((void)(p))
#define debug_unprotect_page(p)	((void)(p))


/* Update the VM system's counts of free pages in the LMM.
   Called at sploskit with phys_lmm_lock held.  */
static void
update_counts (void)
{
  vm_page_unqueued_count = atop (lmm_avail (&malloc_lmm, 0));
}


void *
alloc_for_oskit (oskit_size_t size, osenv_memflags_t flags, unsigned align)
{
  kern_return_t kr;
  lmm_flags_t lmm_flags;

  assert (align <= size);

  if (flags & OSENV_AUTO_SIZE)
    {
      oskit_size_t *p;
      size += sizeof (oskit_size_t);
      if (align > sizeof (oskit_size_t))
	panic ("autosize + align %u", align);
	/* XXX how would I recover the beginning of the block in free?
	size += align - sizeof (oskit_size_t);
	*/
      else
	align = sizeof (oskit_size_t);
      p = alloc_for_oskit (size, flags &~ OSENV_AUTO_SIZE, align);
      if (p == 0)
	return 0;
      p = (void *) (((oskit_addr_t) (p + 1) + align - 1) &~ (align - 1));
      p[-1] = size;
      return p;
    }

  lmm_flags = (((flags & OSENV_ISADMA_MEM) ? LMMF_16MB : 0)
	       | ((flags & OSENV_X861MB_MEM) ? LMMF_1MB : 0));

  if (lmm_flags != 0)
    assert (flags & OSENV_VIRT_EQ_PHYS);

  if (mach_osenv == 0)
    /* This is early oskit startup code before the kernel is set up.
       Always go directly to physical memory.  */
    flags |= OSENV_VIRT_EQ_PHYS|OSENV_PHYS_WIRED|OSENV_PHYS_CONTIG;

  if (in_oskit_interrupt)
    /* The oskit documentation says an interrupt caller must set the flag.  */
    assert (flags & OSENV_NONBLOCKING);

  if (!(flags & (OSENV_NONBLOCKING|OSENV_PHYS_WIRED)))
    {
      /* Unwired virtual memory will do.  */

      if (size < PAGE_SIZE)
	/* For less than a page we don't want to waste partial pages,
	   and we don't want to bother with a second-tier allocator
	   for virtual memory, so just give him wired memory.  */
	return (void *) kalloc (size);
      else
	{
	  vm_offset_t addr;
	  kr = kmem_alloc_pageable (kernel_map, &addr, size);
	  return kr ? 0 : (void *) addr;
	}
    }

  if (!(flags & (OSENV_VIRT_EQ_PHYS|OSENV_NONBLOCKING))
      && (!(flags & OSENV_PHYS_CONTIG) || size <= PAGE_SIZE))
    {
      /* Wired virtual memory will do.  */
      return (void *) kalloc (size);
    }

  /* We need physically contiguous memory.  This we get directly from the
     lmm, which might also be used by vm_page_grab.  All users of the lmm
     must go to sploskit or above and take the phys_lmm_lock.  This blocks
     out any interrupts that might go to the oskit (and get us here).  */
  while (1)
    {
      spl_t s;
      void *block;

      s = sploskit ();
      simple_lock (&phys_lmm_lock);

      if (align)
	block = lmm_alloc_aligned (&malloc_lmm, size, lmm_flags,
				   ffs (align) - 1, 0);
      else
	block = lmm_alloc (&malloc_lmm, size, lmm_flags);

      update_counts ();

      simple_unlock (&phys_lmm_lock);
      splx (s);

      if (block)
	return block;

      if (flags & OSENV_NONBLOCKING)
	return 0;

      /* There is no space in the lmm.  There may well be pages available on
	 the VM system's free list.  If we're sure we are not being called
	 from an interrupt handler here, which we should be guaranteed by
	 checking for OSENV_NONBLOCKING, then we can just access the free
	 list directly here (vm_page_grab).  Another approach is to just wake
	 up the pageout daemon to have it find some pages for the lmm and
	 then block until it wakes us up.  lmm_wants_pages accumulates the
	 number of pages wanted by blocked threads like ourselves.
	 consider_lmm_collect will produce some memory, set lmm_want_pages to
	 zero, and wake up all blocked threads to retry their allocations.  */
      lmm_wants_pages += atop (round_page (size));
      thread_wakeup_one ((event_t)&vm_page_queue_free_count);
      assert_wait ((event_t)&lmm_wants_pages, FALSE);
      thread_block (0);
      /* The pageout daemon woke us up (see consider_lmm_collect)
	 after putting some memory back into the LMM for us.
	 XXX make sure we don't loop in livelock? */
    }
}


void
free_for_oskit (void *block, osenv_memflags_t flags, oskit_size_t size)
{
  if (flags & OSENV_AUTO_SIZE)
    {
      oskit_size_t *p = block;
      size = *--p;
      block = p;
    }

  if (mach_osenv == 0)
    /* This is early oskit startup code before the kernel is set up.
       Always go directly to physical memory.  */
    flags |= OSENV_VIRT_EQ_PHYS|OSENV_PHYS_WIRED|OSENV_PHYS_CONTIG;

  if (in_oskit_interrupt)
    /* The oskit documentation says an interrupt caller must set the flag.  */
    assert (flags & OSENV_NONBLOCKING);

  if ((oskit_addr_t) block < phys_mem_max)
    {
      /* We got physical memory directly from the lmm.  */
      spl_t s;

      s = sploskit ();
      simple_lock (&phys_lmm_lock);

      lmm_free (&malloc_lmm, block, size);
      update_counts ();

      simple_unlock (&phys_lmm_lock);
      splx (s);
    }
  else
    {
      if (in_oskit_interrupt)
	{
	  /* This should be impossible if the caller (oskit) is consistent
	     in its use of OSENV_NONBLOCKING for allocations and
	     deallocations.  alloc_for_oskit will always get physical
	     memory when called with the flag set.  */
	  panic ("free_for_oskit of virtual memory from interrupt level");
	  /* If this ever happens, make a little free list and have the
	     pageout daemon run over it doing kfree.  */
	}
      else
	kfree ((vm_offset_t) block, size);
    }
}


/*** Entry points from the VM system.  ***/


/* Called only from vm_page_bootstrap, only for estimate.  */
unsigned int pmap_free_pages()
{
	return atop (lmm_avail (&malloc_lmm, 0));
}


/* This is called from pmap_bootstrap for kernel page table pages, and from
   pmap_steal_memory for physical pages to back kernel virtual memory
   allocated in early Mach startup.  */
vm_offset_t
pmap_grab_page (void)
{
  void *page = lmm_alloc_page (&malloc_lmm, 0);
  if (page == 0)
    panic ("Not enough memory to initialize Mach");
  return (vm_offset_t) page;
}


/* in zalloc.c XXX */
extern vm_offset_t	zdata;
extern vm_size_t	zdata_size;


void pmap_startup(
	vm_offset_t *startp,
	vm_offset_t *endp)
{
	unsigned int i, npages, initial_pages;
	vm_page_t pages;
	vm_offset_t paddr;

	/* Calculate how many pages can possibly go into the VM pool,
	   taking into account the space required for each page's own
	   vm_page_t structure.  */
	npages = ((lmm_avail (&malloc_lmm, 0) - round_page(zdata_size)) /
		  (PAGE_SIZE + sizeof *pages));

	/* We will allocate a quarter of the pages to start with.
	   That ought to be plenty enough to get the kernel up and running.
	   Once the pageout daemon has started, it will equalize the
	   free page queue and LMM free pool sizes.  */
	initial_pages = npages / 4;

	/* Steal some memory for the zone system, including enough
	   to cover the initial vm_page structures we will steal.  */
	zdata_size = (round_page (virtual_space_start + zdata_size +
				 initial_pages * sizeof *pages)
		      - virtual_space_start);
	zdata = pmap_steal_memory (zdata_size);

	/* Steal vm_page structures from the zone system,
	   and physical pages from the LMM.  */
	pages = (void *) zdata;
	for (i = 0; i < initial_pages; ++i)
	  {
	    vm_page_t mem = pages++;
	    void *page = lmm_alloc_page (&malloc_lmm, 0);
	    vm_page_init (mem, (vm_offset_t) page);
	    debug_protect_page (page);
	    vm_page_release (mem, FALSE);
	  }

	/* Account for the memory swiped for vm_page structures,
	   and return the rest of the stolen memory to the zone system.  */
	zdata_size = zdata + zdata_size - (vm_offset_t) pages;
	zdata = (vm_offset_t) pages;

	/* Set the initial record of the number of pages available in the
	   LMM to a deliberate lie.  This value will be used to set paging
	   algorithm parameters that should be based on what things look
	   like with all available physical pages in the VM pool.  So here
	   we use the calculation that takes into account the overhead of
	   vm_page_t structures that we will have to consume to put pages
	   into the pool.  Counting the free pages in the LMM will show
	   more pages available than we admit, since we haven't allocated
	   all of those structures up front.  As soon as the pageout daemon
	   gets started up, it will recalculate from the LMM.  */
	vm_page_unqueued_count = npages - initial_pages;


	/*
	 *	We have to re-align virtual_space_start,
	 *	because pmap_steal_memory has been using it.
	 */

	virtual_space_start = round_page(virtual_space_start);

	*startp = virtual_space_start;
	*endp = virtual_space_end;
}


void
vm_page_grab_oskit_page (void)
{
  void *page;
  vm_page_t m;

  do
    {
      spl_t s = sploskit ();
      simple_lock (&phys_lmm_lock);

      page = lmm_alloc_page (&malloc_lmm, 0);

      update_counts ();

      debug_protect_page (page);
      simple_unlock (&phys_lmm_lock);
      splx (s);

      if (page == 0)
	panic (__FUNCTION__);

      m = (vm_page_t) zget(vm_page_zone);
      if (m == VM_PAGE_NULL)
	{
	  /* The zone is out of space.  A normal zalloc would call
	     kmem_alloc_wired and wind up recursing down to here trying to
	     get a page.  So instead we just stuff a physical page directly
	     into the zone, and loop for a new one to satisfy our caller.  */
	  debug_unprotect_page (page);
	  zcram(vm_page_zone, (vm_offset_t) page, PAGE_SIZE);
	}
    } while (m == VM_PAGE_NULL);

  vm_page_init(m, (vm_offset_t) page);
  vm_page_release(m, FALSE);
}


#define PAGE_BATCH 32

/* Called by the pageout daemon to see if the vm_page_free_queue
   and the LMM want to tango.  */

void
consider_lmm_collect (void)
{
  spl_t s;
  unsigned int need;

  /* First check if the LMM needs pages back for alloc_for_oskit calls.  */
  if (vm_page_unqueued_count < lmm_wants_pages
      || vm_page_queue_free_count > 8 * vm_page_unqueued_count)	/* random */
    {
      /* The LMM needs some physical pages back from the VM pool.  */
      need = lmm_wants_pages > PAGE_BATCH ? lmm_wants_pages : PAGE_BATCH;

      while (need > 0)
	{
	  unsigned int batch = need > PAGE_BATCH ? PAGE_BATCH : need;
	  void *pages[PAGE_BATCH];
	  unsigned int i;

	  for (i = 0; i < batch; ++i)
	    {
	      vm_page_t mem = vm_page_grab (FALSE);
	      if (mem == VM_PAGE_NULL)
		break;
	      pages[i] = (void *) mem->phys_addr;
	      zfree (vm_page_zone, (vm_offset_t) mem);
	      debug_unprotect_page (pages[i]);
	    }

	  s = sploskit ();
	  simple_lock (&phys_lmm_lock);

	  while (i-- > 0)
	    lmm_free_page (&malloc_lmm, (void *) pages[i]);

	  update_counts ();

	  simple_unlock (&phys_lmm_lock);
	  splx (s);

	  if (i < batch)
	    break;
	  need -= batch;
	}
    }

  /* If lmm_wants_pages is set, then some threads are blocked in
     alloc_for_oskit.  This might still be the case even if the memory was
     released to the LMM before we ran so that we didn't do anything above.
     We need to wake the blocked threads so they will all retry their
     allocations.  */
  if (lmm_wants_pages != 0)
    {
      lmm_wants_pages = 0;
      thread_wakeup ((event_t)&lmm_wants_pages);
    }

  /* Any time the VM pool has fewer free pages than the LMM does,
     we give it half the LMM's free pages.  */

  if (vm_page_queue_free_count < vm_page_unqueued_count)
    {
      s = sploskit ();
      simple_lock (&phys_lmm_lock);

      need = vm_page_unqueued_count / 2;

      /* We allocate only a single page at a time from the LMM, since we
	 don't need contiguous pages, and would rather use up fragmented
	 pages so as to leave the contiguous regions for those who need them.

	 We must go to sploskit to call into the LMM, but back up to spl0 to
	 deliver the new pages to the VM system.  We don't want to do this
	 for every single page allocation from the LMM, so we allocate
	 individual pages from the LMM in batches of PAGE_BATCH pages and
	 then reenable interrupts to deliver those pages before going back
	 for more.  */

      while (need > 0)
	{
	  unsigned int batch = need > PAGE_BATCH ? PAGE_BATCH : need;
	  void *pages[PAGE_BATCH];
	  unsigned int i;

	  for (i = 0; i < batch; ++i)
	    {
	      /* This allocation should never fail, since we keep track
		 of the number of pages available in the lmm.  */
	      pages[i] = lmm_alloc_page (&malloc_lmm, 0);
	      if (pages[i] == 0)
		panic ("LMM unexpectedly out of physical pages");
	      debug_protect_page (pages[i]);
	    }

	  update_counts ();

	  simple_unlock (&phys_lmm_lock);
	  splx (s);

	  while (i-- > 0)
	    vm_page_create ((vm_offset_t) pages[i],
			    (vm_offset_t) pages[i] + PAGE_SIZE);

	  need -= batch;
	  if (need == 0)
	    break;

	  s = sploskit ();
	  simple_lock (&phys_lmm_lock);
	}
    }
}


/*** oskit_osenv_mem_t COM object implementation ***/


/*
 * There is one and only one memory interface in this implementation.
 */
static struct oskit_osenv_mem_ops	osenv_mem_ops;
static struct oskit_osenv_mem		osenv_mem_object = {&osenv_mem_ops};

static OSKIT_COMDECL
mem_query(oskit_osenv_mem_t *s, const oskit_iid_t *iid, void **out_ihandle)
{
        if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
            memcmp(iid, &oskit_osenv_mem_iid, sizeof(*iid)) == 0) {
                *out_ihandle = s;
                return 0;
        }

        *out_ihandle = 0;
        return OSKIT_E_NOINTERFACE;
};

static OSKIT_COMDECL_U
mem_addref(oskit_osenv_mem_t *s)
{
	/* Only one object */
	return 1;
}

static OSKIT_COMDECL_U
mem_release(oskit_osenv_mem_t *s)
{
	/* Only one object */
	return 1;
}

static void * OSKIT_COMCALL
mem_alloc(oskit_osenv_mem_t *o, oskit_size_t size,
	  osenv_memflags_t flags, unsigned align)
{
  return alloc_for_oskit (size, flags, align);
}

static OSKIT_COMDECL_V
mem_free(oskit_osenv_mem_t *o, void *block,
	 osenv_memflags_t flags, oskit_size_t size)
{
  free_for_oskit (block, flags, size);
}

static oskit_addr_t OSKIT_COMCALL
mem_getphys(oskit_osenv_mem_t *o, oskit_addr_t va)
{
  /* We can only use kvtophys for memory allocated with OSENV_PHYS_CONTIG.
     But at least some code (e.g. eepro100 driver) will call us with an
     address on a kernel stack and expect to get a physical address from that.
     Checking the kernel pmap is a little heavy but should be correct in
     all cases.  */
  return pmap_extract (kernel_pmap, va);
}

static oskit_addr_t OSKIT_COMCALL
mem_getvirt(oskit_osenv_mem_t *o, oskit_addr_t pa)
{
	return ((oskit_addr_t)phystokv(pa));
}

static oskit_addr_t OSKIT_COMCALL
mem_physmax(oskit_osenv_mem_t *o)
{
	return phys_mem_max;
}

static OSKIT_COMDECL_U
mem_mapphys(oskit_osenv_mem_t *o, oskit_addr_t pa,
	    oskit_size_t size, void **addr, int flags)
{
  kern_return_t kr;
  vm_offset_t kva;

  if (trunc_page (pa) != 0 &&
      trunc_page (pa) >= trunc_page (phys_mem_min) &&
      round_page (pa + size) < round_page (phys_mem_max))
    {
      /* Direct mapping already covers it.  */
      *addr = (void *) phystokv (pa);
      return 0;
    }

  kr = kmem_alloc_pageable (kernel_map, &kva, round_page (size));
  if (kr)
    return OSKIT_E_OUTOFMEMORY;

  pmap_map (kva, trunc_page (pa), round_page (pa + size),
	    VM_PROT_READ | VM_PROT_WRITE);

  *addr = (char *) kva + (pa & PAGE_MASK);
  return 0;
}

#if NCPUS > 1
/* This function is used as a callback from smp_init_paging.  */
oskit_addr_t
smp_map_range (oskit_addr_t start, oskit_size_t size)
{
  oskit_error_t err = mem_mapphys (0, start, size, &start, 0);
  return err ? 0 : start;
}
#endif


static struct oskit_osenv_mem_ops osenv_mem_ops = {
	mem_query,
	mem_addref,
	mem_release,
	mem_alloc,
	mem_free,
	mem_getphys,
	mem_getvirt,
	mem_physmax,
	mem_mapphys,
};

/*
 * Return a reference to the one and only memory interface object.
 */
oskit_osenv_mem_t *
oskit_create_osenv_mem(void)
{
	return &osenv_mem_object;
}


/*** oskit_mem_t COM object implementation ***/


/* the COM object */
static struct oskit_mem_ops mem_ops;
static oskit_mem_t	oskit_mem  = { &mem_ops };


static OSKIT_COMDECL
mem2_query(oskit_mem_t *m, const oskit_iid_t *iid, void **out_ihandle)
{
        if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
            memcmp(iid, &oskit_mem_iid, sizeof(*iid)) == 0) {
                *out_ihandle = m;
                return 0;
        }

        *out_ihandle = 0;
        return OSKIT_E_NOINTERFACE;
};

static OSKIT_COMDECL_U
mem2_addref(oskit_mem_t *m)
{
	/* No reference counting; only one of them */
	return 1;
}

static OSKIT_COMDECL_U
mem2_release(oskit_mem_t *m)
{
	/* No reference counting; only one of them */
	return 1;
}

static void * OSKIT_COMCALL
mem2_alloc(oskit_mem_t *m, oskit_u32_t size, oskit_u32_t flags)
{
  return alloc_for_oskit (size, flags, 0);
}

static void * OSKIT_COMCALL
mem2_realloc(oskit_mem_t *m, void *ptr,
	    oskit_u32_t oldsize, oskit_u32_t newsize, oskit_u32_t flags)
{
  if (flags & OSKIT_MEM_AUTO_SIZE)
    {
      oskit_size_t *op = ptr, *new;
      oldsize = *--op;
      newsize += sizeof (oskit_size_t);
      new = alloc_for_oskit (newsize, flags, 0);
      if (new)
	{
	  *new++ = newsize;
	  memcpy (new, ptr, oldsize - sizeof (oskit_size_t));
	  free_for_oskit (op, flags, oldsize);
	}
      return new;
    }
  else
    {
      void *new = alloc_for_oskit (newsize, flags, 0);
      if (new)
	{
	  memcpy (new, ptr, oldsize);
	  free_for_oskit (ptr, flags, oldsize);
	}
      return new;
    }
}

static void *OSKIT_COMCALL
mem2_alloc_aligned(oskit_mem_t *m,
		  oskit_u32_t size, oskit_u32_t flags, oskit_u32_t align)
{
  return alloc_for_oskit (size, flags, align);
}

static void OSKIT_COMCALL
mem2_free(oskit_mem_t *m, void *ptr, oskit_u32_t size, oskit_u32_t flags)
{
  if (ptr)
    free_for_oskit (ptr, flags, size);
}

static oskit_u32_t OSKIT_COMCALL
mem2_getsize(oskit_mem_t *m, void *ptr)
{
	if (ptr) {
		oskit_u32_t *chunk = (oskit_u32_t *)ptr - 1;

		return *chunk;
	}
	return 0;
}

static void *OSKIT_COMCALL
mem2_alloc_gen(oskit_mem_t *m,
	      oskit_u32_t size, oskit_u32_t flags,
	      oskit_u32_t align_bits, oskit_u32_t align_ofs)
{
  if (align_ofs != 0)
    panic ("alloc with align_bits=%u align_ofs=%u",
	   align_bits, align_ofs);
  return alloc_for_oskit (size, flags, 1 << align_bits);
}

static oskit_size_t OSKIT_COMCALL
mem2_avail(oskit_mem_t *m, oskit_u32_t flags)
{
  panic ("mem_avail");
}

static OSKIT_COMDECL_V
mem2_dump(oskit_mem_t *m)
{
  panic ("mem_dump");
}

static struct oskit_mem_ops mem_ops = {
	mem2_query,
	mem2_addref,
	mem2_release,
	mem2_alloc,
	mem2_realloc,
	mem2_alloc_aligned,
	mem2_free,
	mem2_getsize,
	mem2_alloc_gen,
	mem2_avail,
	mem2_dump,
};

/*
 * Not a lot to do in this impl.
 */
oskit_mem_t *
oskit_mem_init(void)
{
	return &oskit_mem;
}
