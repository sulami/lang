#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

/*
 * Init runtime
 */

void init_nebula() {
  puts("Nebula initalised.");
  srand(time(NULL));
}

/*
 * Testing
 */

bool random_bool() {
  return (rand() % 2);
}

char* read_file(const char* file_name, const char* mode) {
  // TODO all of this is horrible, fix this up at some point.
  const int buf_size = 255;
  char* buf = calloc(buf_size, sizeof(char));
  FILE* fp = fopen(file_name, mode);
  fgets(buf, buf_size, fp);
  fclose(fp);
  return buf;
}

/*
 * Cons cells
 */

struct cons_cell {
  void* car;
  struct cons_cell* cdr;
};

struct cons_cell* cons(void* head, struct cons_cell* tail) {
  if (NULL == head) {
    return NULL;
  }

  struct cons_cell* new_cons = malloc(sizeof(struct cons_cell));
  if (NULL == new_cons) {
    exit(ENOMEM);
  }

  new_cons->car = head;
  new_cons->cdr = tail;

  return new_cons;
}

void* car(struct cons_cell* c) {
  return c->car;
}

struct cons_cell* cdr(struct cons_cell* c) {
  return c->cdr;
}

/*
 * Association lists
 */

struct cons_cell* alist(void* key, void* value, struct cons_cell* tail) {
  if ((NULL == key) || (NULL == value)) {
    return NULL;
  }

  struct cons_cell* new_value = cons(value, NULL);
  struct cons_cell* new_key = cons(key, new_value);
  return cons(new_key, tail);
}

void* aget(void* key, struct cons_cell* list) {
  struct cons_cell* head = list;

  while (NULL != head) {
    if (key == car(car(list))) {
      return cdr(car(list));
    }
    head = cdr(list);
  }

  return NULL;
}

void* unbox(void** ptr) {
  return *ptr;
}
