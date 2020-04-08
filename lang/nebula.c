#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

/*
 * Testing
 */

bool random_bool() {
  return (rand() % 2);
}

bool not(bool in) {
  return (in != 1);
}

void* unbox(void** ptr) {
  return *ptr;
}

/*
 * Runtime
 */

void init_nebula() {
  puts("Nebula initalised.");
  srand(time(NULL));
}

/*
 * Values (the runtime understanding)
 */

// Primitive values are <100.
enum Type {NIL = 0, BOOL = 1, INT = 2, FLOAT = 3, STRING = 100};

struct Value {
  enum Type type;
  void* value;
};

struct Value* make_value(enum Type type, void* value) {
  // TODO don't double-allocate nil
  // TODO box all but strings
  struct Value* retval = malloc(sizeof(struct Value));
  if (NULL == retval) {
    exit(ENOMEM);
  }
  retval->type = type;
  retval->value = value;
  return retval;
}

enum Type value_type(struct Value* value) {
  return value->type;
}

void* unbox_value(struct Value* value) {
  return value->value;
}

/*
 * I/O
 */

char* read_file(const char* file_name, const char* mode) {
  char* buffer;
  FILE* fp = fopen(file_name, mode);
  if (NULL == fp) {
    exit(ENOMEM);
  }
  fseek(fp, 0L, SEEK_END);
  long s = ftell(fp);
  rewind(fp);
  buffer = malloc(s);
  if (NULL == buffer) {
    fclose(fp);
    exit(ENOMEM);
  }
  fread(buffer, s, 1, fp);
  fclose(fp);
  return buffer;
}

void print_bool(bool b) {
  printf("bool is %d\n", b);
}

void print_int(int b) {
  printf("int is %d\n", b);
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
