#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

int usercode_main(int, char**);
struct Value* cdr(struct Value*);
struct Value* car(struct Value*);

/*
 * Runtime
 */

/* void repl() { */
/*   char* prompt = "> "; */
/*   char* input = NULL; */
/*   size_t len; */
/*   while (true) { */
/*     getline(&input, &len, stdin); */
/*     puts(input); */
/*     free(input); */
/*   } */
/* } */

void init_nebula() {
  puts("Nebula initalised.");
  srand(time(NULL));
}

/* This is the runtime main function. It will init the runtime and
then pass control to the implicit user code main function.

The call order is like this:
LLVM main -> nebula_main -> usercode_main
           ↪ init_nebula
 */
int nebula_main(int argc, char** argv) {
  init_nebula();
  /* for (int i = 0; i < argc; i++) { */
  /*   puts(argv[i]); */
  /* } */
  // XXX Forward command line arguments to usercode for now.
  return usercode_main(argc, argv);
}

/*
 * Values (the runtime understanding)
 */

// Primitive values are <100.
enum Type {NIL = 0, BOOL = 1, INT = 2, FLOAT = 3,
           STRING = 100, CONS = 101};
union Primitive {
  bool b;
  int i;
  double f;
  char* string;
  struct cons_cell* cons;
};

struct Value {
  enum Type type;
  union Primitive* value;
};

struct Value* make_value(enum Type type, union Primitive* value) {
  // TODO don't double-allocate nil
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

bool value_equal(struct Value* a, struct Value* b) {
  if (a->type != b->type) {
    return false;
  }
  switch (a->type) {
  case NIL:
    return true;
  case BOOL:
    return (a->value->b == b->value->b);
  case INT:
    return (a->value->i == b->value->i);
  case FLOAT:
    return (a->value->f == b->value->f);
  case STRING:
    return (0 == strcmp((char*)a->value, (char*)b->value));
  case CONS:
    return (value_equal(car(a), car(b)) && value_equal(cdr(a), cdr(b)));
  }
  return false;
}

union Primitive* unbox_value(struct Value* value) {
  return value->value;
}

void print_value(struct Value* value) {
  switch (value->type) {
  case NIL:
    printf("nil");
    break;
  case BOOL:
    printf("%s", (1 == value->value->b) ? "true" : "false");
    break;
  case INT:
    printf("%d", value->value->i);
    break;
  case FLOAT:
    printf("%f", value->value->f);
    break;
  case STRING:
    printf("%s", (char *)(value->value));
    break;
  case CONS:
    printf("(");
    print_value(car(value));
    printf(" ");
    print_value(cdr(value));
    printf(")");
    break;
  }
}

/*
 * I/O
 */

struct Value* read_file(const struct Value* name, const struct Value* m) {
  const char* file_name = (char*)name->value;
  const char* mode = (char*)m->value;
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
  return make_value(STRING, (union Primitive*)buffer);
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
  struct Value* car;
  struct Value* cdr;
};

struct Value* cons(struct Value* head, struct Value* tail) {
  if (NULL == head) {
    return make_value(NIL, NULL);
  }

  struct cons_cell* new_cons = malloc(sizeof(struct cons_cell));
  if (NULL == new_cons) {
    exit(ENOMEM);
  }

  new_cons->car = head;
  new_cons->cdr = tail;

  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->cons = new_cons;
  return make_value(CONS, u);
}

struct Value* car(struct Value* c) {
  union Primitive* value = c->value;
  if (NULL == value) {
    return make_value(NIL, NULL);
  }
  struct cons_cell* co = value->cons;
  if (NULL == co) {
    return make_value(NIL, NULL);
  }
  return co->car;
}

struct Value* cdr(struct Value* c) {
  union Primitive* value = c->value;
  if (NULL == value) {
    return make_value(NIL, NULL);
  }
  struct cons_cell* co = value->cons;
  if (NULL == co) {
    return make_value(NIL, NULL);
  }
  return co->cdr;
}

/*
 * Association lists
 */

struct Value* alist(struct Value* key, struct Value* value, struct Value* tail) {
  if ((NULL == key) || (NULL == value)) {
    return make_value(NIL, NULL);
  }

  struct Value* new_key_value_pair = cons(key, value);
  return cons(new_key_value_pair, tail);
}

struct Value* aget(struct Value* key, struct Value* list) {
  struct Value* head = list;

  while (NIL != head->type) {
    if (value_equal(key, car(car(head)))) {
      return cdr(car(head));
    }
    head = cdr(head);
  }

  return make_value(NIL, NULL);
}

/*
 * Testing
 */

struct Value* random_bool() {
  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->b = rand() % 2;
  return make_value(BOOL, u);
}

bool not(bool in) {
  return (in != 1);
}

void* unbox(void** ptr) {
  return *ptr;
}
