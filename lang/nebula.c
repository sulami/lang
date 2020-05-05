#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <llvm-c/Core.h>

int usercode_main(int, char**);
struct Value* cons(struct Value*, struct Value*);
struct Value* cdr(struct Value*);
struct Value* car(struct Value*);

/* Runtime */

void init_nebula() {
  srand(time(NULL));
  puts("Nebula initalised.");
}

/* This is the runtime main function. It will init the runtime and
then pass control to the implicit user code main function.

The call order is like this:
LLVM main -> nebula_main -> usercode_main
           ↪ init_nebula
 */
int nebula_main(int argc, char** argv) {
  init_nebula();
  return usercode_main(argc, argv);
}

/* Values (the runtime understanding) */

// Primitive values are <128, pointers >=128.
enum Type {NIL = 0, BOOL = 1, INT = 2, FLOAT = 3, CHAR = 4, TYPE = 5,
           STRING = 128, CONS = 129, FUNCTION = 130};
union Primitive {
  bool b;
  int i;
  float f;
  void* ptr;
};

struct Value {
  enum Type type;
  union Primitive* value;
};

struct Cons {
  struct Value* car;
  struct Value* cdr;
};

struct Function {
  char* name;
  void* fn_ptr;
};

struct Value* make_value(enum Type type, union Primitive* value) {
  struct Value* retval = malloc(sizeof(struct Value));
  if (NULL == retval) {
    exit(ENOMEM);
  }

  /* printf("making value - ptr: %X - type: %d\n", retval, type); */

  // We have to allocate the actual value on the heap, as LLVM stores
  // things on the stack only, so we'd lose our boxed value when
  // returning from a function. Nil is a special case, as value is
  // empty, and (usually) a null pointer, so we can skip this step.
  // Non-primitive values (type >= 128) can also safely be assigned
  // over, as they're only pointers anyway.
  if ((NIL != type) && (type < 128)) {
    union Primitive* val = malloc(sizeof(union Primitive));
    if (NULL == val) {
      exit(ENOMEM);
    }
    *val = *value;
    retval->value = val;
  } else {
    retval->value = value;
  }

  /* printf(" primitive: %X\n", retval->value); */
  retval->type = type;
  return retval;
}

enum Type value_type(struct Value* value) {
  return value->type;
}

struct Value* make_function(char* name, void* fn_ptr) {
  struct Function* f = malloc(sizeof(struct Function));
  if (NULL == f) {
    exit(ENOMEM);
  }
  size_t name_length = strlen(name);
  char* new_name = malloc(name_length * sizeof(char));
  if (NULL == new_name) {
    exit(ENOMEM);
  }
  strcpy(new_name, name);
  f->name = new_name;
  f->fn_ptr = fn_ptr;
  /* printf("making function - fn name: %s; fn ptr: %X; struct ptr: %X\n", f->name, f->fn_ptr, f); */

  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->ptr = f;
  return make_value(FUNCTION, u);
}

struct Value* value_equal(struct Value* a, struct Value* b) {
  bool result;

  if (a->type != b->type) {
    result = false;
  } else {
    switch (a->type) {
    case NIL:
      result = true;
      break;
    case BOOL:
      result = (a->value->b == b->value->b);
      break;
    case INT:
      result = (a->value->i == b->value->i);
      break;
    case FLOAT:
      result = (a->value->f == b->value->f);
      break;
    case CHAR:
      result = (a->value->i == b->value->i);
      break;
    case TYPE:
      result = (a->value->i == b->value->i);
      break;
    case STRING:
      result = (0 == strcmp((char*)a->value, (char*)b->value));
      break;
    case CONS:
      result = (value_equal(car(a), car(b)) && value_equal(cdr(a), cdr(b)));
      break;
    case FUNCTION:
      result = (((struct Function*)a->value->ptr)->name == ((struct Function*)b->value->ptr)->name);
      break;
    default:
      result = false;
      break;
    }
  }

  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->b = result;
  return make_value(BOOL, u);
}

bool value_truthy(struct Value* value) {
  switch (value->type) {
  case NIL:
    return false;
    break;
  case BOOL:
    return value->value->b;
    break;
  case INT:
    return 0 < value->value->i;
    break;
  case FLOAT:
    return 0 < value->value->f;
    break;
  case CHAR:
    return true;
    break;
  case TYPE:
    return true;
    break;
  case STRING:
    return 0 < strlen((char *)value->value);
    break;
  case CONS:
    return NIL != value->value->ptr;
    break;
  case FUNCTION:
    return true;
    break;
  default:
    return false;
    break;
  }
}

union Primitive* unbox_value(struct Value* value) {
  return value->value;
}

struct Value* type(struct Value* value) {
  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->i = value->type;
  return make_value(TYPE, u);
}

void print_value(struct Value* value) {
  /* printf("print_value: %d: %X\n", value->type, value->value); */
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
  case CHAR:
    if (10 == value->value->i) {
      printf("\\newline");
    } else if (32 == value->value->i) {
      printf("\\space");
    } else {
      printf("\\%c", value->value->i);
    }
    break;
  case TYPE:
    switch (value->value->i) {
    case NIL:
      printf("<type: nil>");
      break;
    case BOOL:
      printf("<type: bool>");
      break;
    case INT:
      printf("<type: int>");
      break;
    case FLOAT:
      printf("<type: float>");
      break;
    case CHAR:
      printf("<type: char>");
      break;
    case TYPE:
      printf("<type: type>");
      break;
    case STRING:
      printf("<type: string>");
      break;
    case CONS:
      printf("<type: cons>");
      break;
    case FUNCTION:
      printf("<type: function>");
      break;
    default:
      printf("<type: unknown>");
      break;
    }
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
  case FUNCTION:
    printf("<function: %s>", ((struct Function*)value->value->ptr)->name);
    break;
  }
}

/* Strings */

struct Value* concat_strings(const struct Value* a, const struct Value* b) {
  const char* astring = (char*)a->value;
  const char* bstring = (char*)b->value;
  const size_t total_len = 1 + strlen(astring) + strlen(bstring);
  char* cstring = malloc(total_len * sizeof(char));
  if (NULL == cstring) {
    exit(ENOMEM);
  }
  snprintf(cstring, total_len, "%s%s", astring, bstring);
  return make_value(STRING, (union Primitive*)cstring);
}

struct Value* string_to_cons(const struct Value* s) {
  const char* str = (char*)s->value;
  struct Value* retval = cons(NULL, NULL);
  size_t len = strlen(str);
  for (size_t i = len; 0 < i; --i) {
    union Primitive* u = calloc(1, sizeof(union Primitive));
    u->i = *(str+i-1);
    retval = cons(make_value(CHAR, u) , retval);
  }
  return retval;
}

struct Value* cons_to_string(const struct Value* l) {
  size_t len = 0;
  const struct Value* head = l;
  // Walk once to get the length.
  while (NIL != head->value) {
    ++len;
    head = ((struct Cons*)head->value->ptr)->cdr;
  }
  char* str = malloc((1 + len) * sizeof(char));
  if (NULL == str) {
    exit(ENOMEM);
  }
  head = l;
  // Walk again to fill the string.
  for (size_t pos = 0; pos < len; ++pos) {
    *(str+pos) = (char)((struct Cons*)head->value->ptr)->car->value->i;
    /* sprintf((char*), str + pos); */
    head = ((struct Cons*)head->value->ptr)->cdr;
  }
  return make_value(STRING, (union Primitive*)str);
}

/* I/O */

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

struct Value* write_file(const struct Value* name,
                         const struct Value* m,
                         const struct Value* s) {
  const char* file_name = (char*)name->value;
  const char* mode = (char*)m->value;
  const char* str =  (char*)s->value;
  FILE* fp = fopen(file_name, mode);
  if (NULL == fp) {
    exit(ENOMEM);
  }
  fputs(str, fp);
  fclose(fp);
  return make_value(NIL, NULL);
}

/* Cons cells */

struct Value* cons(struct Value* head, struct Value* tail) {
  if (NULL == head) {
    return make_value(NIL, NULL);
  }

  struct Cons* new_cons = malloc(sizeof(struct Cons));
  if (NULL == new_cons) {
    exit(ENOMEM);
  }

  new_cons->car = head;
  new_cons->cdr = tail;

  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->ptr = new_cons;
  return make_value(CONS, u);
}

struct Value* car(struct Value* c) {
  union Primitive* value = c->value;
  if (NULL == value) {
    return make_value(NIL, NULL);
  }
  struct Cons* co = value->ptr;
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
  struct Cons* co = value->ptr;
  if (NULL == co) {
    return make_value(NIL, NULL);
  }
  return co->cdr;
}

/* Association lists */

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

/* Testing */

struct Value* random_bool() {
  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->b = rand() % 2;
  return make_value(BOOL, u);
}

void nebula_debug(void* x) {
  printf("[DEBUG] base 10: %u; base 16: %X\n", (unsigned int)x, (unsigned int)x);
}
