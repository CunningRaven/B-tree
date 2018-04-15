#include <stdio.h>
#include "syscall_fail.h"

void syscall_fail(const char *syscall_name)
{
  perror(syscall_name);
}

