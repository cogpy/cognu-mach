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