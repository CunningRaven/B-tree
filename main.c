#include <stdio.h>
#include <stdlib.h>

void syscall_fail(const char *syscall_name)
{
  perror(syscall_name);
  exit(1);
}

int main(void)
{
  return 0;
}

