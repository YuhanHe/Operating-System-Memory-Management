# Operating-System-Memory-Management

## Introduction
In this assignment you will implement the virtual memory sub-system of OS/161. The existing VM implementation in OS/161, dumbvm, is a minimal implementation with a number of shortcomings. In this assignment you will adapt OS/161 to take full advantage of the simulated hardware by implementing management of the MIPS software-managed Translation Lookaside Buffer (TLB). You will write the code to manage this TLB.

#### The System/161 TLB
In the System/161 machine, each TLB entry includes a 20-bit virtual page number and a 20-bit physical page number as well as the following five fields:
global: 1 bit; if set, ignore the PID bits in the TLB.
valid: 1 bit; set if the TLB entry contains a valid translation.
dirty: 1 bit; enables writing to the page referenced by the entry; if this bit is 0, the page is only accessible for reading.
nocache: 1 bit; unused in System/161. In a real processor, indicates that the hardware cache will be disabled when accessing this page.
pid: 6 bits; a context or address space ID that can be used to allow entries to remain in the TLB after a context switch.
All these bits/values are maintained by the operating system. When the valid bit is set, the TLB entry contains a valid translation. This implies that the virtual page is present in physical memory. A TLB miss occurs when no TLB entry can be found with a matching virtual page and address space ID (unless the global bit is set in which case the address space ID is ignored) and a valid bit that is set.

For this assignment, you may ignore the pid field. Note, however, that you must then flush the TLB on a context switch (why?).

#### The System/161 Virtual Address Space Map
The MIPS divides its address space into several regions that have hardwired properties. These are:
kseg2, TLB-mapped cacheable kernel space
kseg1, direct-mapped uncached kernel space
kseg0, direct-mapped cached kernel space
kuseg, TLB-mapped cacheable user space
Both direct-mapped segments map to the first 512 megabytes of the physical address space.

The top of kuseg is 0x80000000. The top of kseg0 is 0xa0000000, and the top of kseg1 is 0xc0000000.

The memory map thus looks like this:

| Address | Segment | Special properties |
| :-----| :-----  | :----- |
| 0xffffffff | kseg2 |  |
| 0xc0000000 | kseg2 |  |
|0xbfffffff|kseg1|  |
|0xbfc00180|kseg1|Exception address if BEV set.  |
|0xbfc00100|kseg1|UTLB exception address if BEV set.  |
|0xbfc00000|kseg1| Execution begins here after processor reset. |
|0xa0000000|kseg1|  |
|0x9fffffff|kseg0||
|0x80000080|kseg0|Exception address if BEV not set.|
|0x80000000|kseg0|UTLB exception address if BEV not set.|
|0x7fffffff|kuseg||
|0x00000000|kuseg||
 
## Setting Up Assignment 3
We assume after ASST0, ASST1, and ASST2 that you now have some familiarity with setting up for OS/161 development. If you need more detail, refer back to ASST0.

Clone the ASST3 source repository from gitlab.cse.unsw.edu.au. Note: replace XXX with your 3 digit group number.

```% cd ~/cs3231```<br>
```% git clone gitlab@gitlab.cse.unsw.EDU.AU:19t1-comp3231-grpXXX/asst3.git asst3-src```<br>
Note: The gitlab repository is shared between you and your partner. You can both push and pull changes to and from the repository to cooperate on the assignment. If you are not familiar with cooperative software development and git you should consider spending a little time familiarising yourself with git.

#### Configure OS/161 for Assignment 3
Remember to set your PATH environment variable as in previous assignments (e.g. run the 3231 command).

Before proceeding further, configure your new sources, and build and install the user-level libraries and binaries.

```% cd ~/cs3231/asst3-src```<br>
```% ./configure```<br>
```% bmake```<br>
```% bmake install```<br>
You have to reconfigure your kernel before you can use the framework provided to do this assignment. The procedure for configuring a kernel is the same as before, except you will use the ASST3 configuration file:

```% cd ~/cs3231/asst3-src/kern/conf```<br>
```% ./config ASST3```<br>
You should now see an ASST3 directory in the compile directory.

#### Building for ASST3
When you built OS/161 for ASST0, you ran bmake from compile/ASST0. When you built for ASST1, you ran bmake from compile/ASST1 ... you can probably see where this is heading:

```% cd ../compile/ASST3```<br>
```% bmake depend```<br>
```% bmake```<br>
```% bmake install```<br>
If you now run the kernel as you did for previous assignments, you should get to the menu prompt. If you try and run a program, it will fail with a message about an unimplemented feature (the failure is due to the unimplemented as_* functions that you must write). For example, run ```p /bin/true``` at the OS/161 prompt to run the program ```/bin/true in ~/cs3231/root```.

```OS/161 kernel [? for menu]: p /bin/true```<br>
```Running program /bin/true failed: Function not implemented```<br>
```Program (pid 2) exited with status 1```<br>
```Operation took 0.173469806 seconds```<br>
```OS/161 kernel [? for menu]:```<br>


Note: If you don't have a sys161.conf file, you can use the one from ASST1.

The simplest way to install it is as follows:<br>
```% cd ~/cs3231/root```<br>
```% wget http://cgi.cse.unsw.edu.au/~cs3231/19T1/assignments/asst1/sys161.conf -O sys161.conf```<br>
You are now ready to start the assignment.

## Coding Assignment
This assignment involves designing and implementing a number of data-structures and the functions that manipulate them. Before you start, you should work out what data you need to keep track of, and what operations are required.

#### Address Space Management
OS/161 has an address space data type that encapsulates the book-keeping needed to describe an address space: the struct addrspace. To enable OS/161 to interact with your VM implementation, you will need to implement in the functions in kern/vm/addrspace.c and potentialy modify the data type. The semantics of these functions is documented in kern/include/addrspace.h.

Note: You may use a fixed-size stack region (say 16 pages) for each process.

#### Address Translation
The main goal for this assignment is to provide virtual memory translation for user programs. To do this, you will need to implement a TLB refill handler. You will also need to implement a page table. For this assignment, you will implement a 2-level hierarchical page table.

Note that a hierarchical page table is a lazy data-structure. This means that the contents of the page table, including the second level pages, are only allocated when they are needed. You may find allocating the required pages at load time helps you start your assignment, however, your final solution should allocate pages only when a page-fault occurs.

The following questions may assist you in designing the contents of your page table

What information do you need to store for each page?
How does the page table get populated?
Note: Applications expect pages to contain zeros when first used. This implies that newly allocated frames that are used to back pages should be zero-filled prior to mapping.

#### Testing and Debugging Your Assignment
To test this assignment, you should run a process that requires more virtual memory than the TLB can map at any one time. You should also ensure that touching memory not in a valid region will raise an exception. The huge and faulter tests in testbin may be useful. See the Wiki for more options.

Apart from GDB, you may also find the trace161 command useful. trace161 will run the simulator with tracing, for example

```% trace161 -t t -f outfile kernel```<br>
will record all TLB accesses in outfile.
Don't use kprintf() for vm_fault() debugging. See Wiki for more info.

Hints
To implement a page table, have a close look at the dumbvm implementation, especially vm_fault(). Although it is simple, you should get an idea on how to approach the rest of the assignment.

One approach to implementing the assignment is in the following order:

Understand how the page table works, and its relationship with the TLB.
Understand the specification and the supplied code.
Work out a basic design for your page table implementation.
Modify kern/vm/vm.c to insert , lookup, and update page table entries, and keep the TLB consistent with the page table.
Implement the TLB exception handlers in vm.c using your page table.
Implement the functions in kern/vm/addrspace.c that are required for basic functionality (e.g. as_create(), as_prepare_load(), etc.). Allocating user pages in as_define_region() may also simplify your assignment, however good solution allocate pages in vm_fault().
Test and debug this. Use the debugger! 
If you really get stuck, submit at least this much of the solution, and you should get some marks for it.
Note: Interrupts should be disabled when writing to the TLB, see dumbvm for an example. Otherwise, unexpected concurrency issues can occur.

as_activate() and as_deactivate() can be copied from dumbvm.
		 
	
	
	
	 


		 
