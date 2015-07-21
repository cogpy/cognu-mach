/*
 * Mach Kernel - GDB Stub Code
 * Copyright (c) 2007 Free Software Foundation
 */

#include <mach/std_types.h>
#include <sys/types.h>

/** 
 * We don't have a header file for the com
 * functions, so extern them here, and talk
 * about on bug-hurd about making the header
 */

extern int comcnputc(dev_t dev, int c);
extern int comcngetc(dev_t dev, int wait);

/**
 * putDebugChar puts a character over the serial port
 * for the GDB stub. Its prototyped in stub-i386.c
 */

void putDebugChar(int character) 
{
  comcnputc(0, character);
}

/**
 * getDebugChar gets a character over the serial port
 * for the GDB stub. Its prototyped in stub-i386.c.
 */

int getDebugChar()
{
  return comcngetc(0, TRUE);
}
