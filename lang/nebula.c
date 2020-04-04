#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

void init_nebula() {
  puts("Nebula initalised.");
}

void print(char* s) {
  puts(s);
}

char* read_file(const char* file_name, const char* mode) {
  const int buf_size = 255;
  char* buf = malloc(sizeof(char) * buf_size);
  FILE* fp = fopen(file_name, mode);
  fgets(buf, buf_size, fp);
  fclose(fp);
  return buf;
}

bool random_bool() {
  srand(1);
  return rand() % 2;
}
