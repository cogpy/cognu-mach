/*
 *  Copyright (C) 2024 Free Software Foundation
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <testlib.h>

int main(int argc, char *argv[], int envc, char *envp[])
{
  printf("=== Console Timestamp Feature Test ===\n");
  
  /* Test that timestamps are enabled by default */
  ASSERT(console_timestamp_is_enabled(), "Timestamps should be enabled by default");
  
  /* Test basic printf with timestamps */
  printf("Testing basic message output\n");
  printf("Multiple line output:\n");
  printf("Line 1\n");
  printf("Line 2\n");
  printf("Line 3\n");
  
  /* Test timestamp disable/enable functionality */
  printf("Disabling timestamps...\n");
  console_timestamp_enable(FALSE);
  ASSERT(!console_timestamp_is_enabled(), "Timestamps should be disabled");
  
  printf("This message should have no timestamp\n");
  printf("Neither should this one\n");
  
  printf("Re-enabling timestamps...\n");
  console_timestamp_enable(TRUE);
  ASSERT(console_timestamp_is_enabled(), "Timestamps should be re-enabled");
  
  printf("This message should have timestamps again\n");
  printf("And so should this one\n");
  
  /* Test mixed output */
  printf("Testing mixed content: ");
  printf("same line continuation\n");
  
  printf("%s: %s\n", TEST_SUCCESS_MARKER, "Console timestamp test completed successfully");
  
  return 0;
}