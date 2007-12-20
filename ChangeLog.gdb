2007-12-20 Michael Casadevall <sonicmctails@gmail.com>
	   * i386/i386/locore.S - Modified a condition to also
	   include MACH_GDB_STUB
	   * kern/bootstrap.c - Moved call to set_debug_traps()
	   * kern/startup.c - Moved call here for set_debug_traps
	   * i386/i386/gdb-stub.c - Isolated the crash in the stub
	   to an iret statement

2007-12-19 Michael Casadevall <sonicmctails@gmail.com>
	   * configfrag.ac - Added GDB stub option.
	   * Makefrag.am - Likewise.
	   * i386/Makefrag.am - Likewise.
	   * i386/i386/com.c - Modified an ifdef so console serial port
	   functions are available when MACH_GDB_STUB is set
	   * i386/i386/gdb-stub.c - New File. GDB stub for remote debugging
	   * kern/gdb.c - New File. Has the stub's support functions
	   * kern/bootstrap.c - Has inital breakpoint to enter GDB stub
	   * kern/debug.c - Modified to allow panic() to enter GDB stub