#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

char buff[2][BUFSIZ];

int main(int argc, char *argv[])
{
  size_t sz[2];
  int is_key[2] = { 0, 0 };
  int line_no = 0;

  if (argc < 2)
    return 1;
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    perror("fopen");
    exit(1);
  }
  int no = 0;
  while (fgets(buff[no], BUFSIZ, fp) != NULL) {
    sz[no] = strlen(buff[no]);
    if (sz[no] >= 5 && buff[no][sz[no]-5] == ' ' && buff[no][sz[no]-4] == '4' && buff[no][sz[no]-3] == '2' && buff[no][sz[no]-2] == '4') {
      is_key[no] = 1;
      if (is_key[no ^ 1] && sz[no ^ 1] < sz[no]) {
        printf("%d\n", ++line_no);
        exit(1);
      }
    } else {
      is_key[no] = 0;
    }
    no ^= 1;
    line_no++;
  }
  return 0;
}

