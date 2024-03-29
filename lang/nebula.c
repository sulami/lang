#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

int usercode_main(int, char**);
struct Value* cons(struct Value*, struct Value*);
struct Value* cdr(struct Value*);
struct Value* car(struct Value*);

void nebula_debug(void* x) {
  printf("[DEBUG] base 10: %u; base 16: %X\n", (unsigned int)x, (unsigned int)x);
}

/* Runtime */

void init_nebula() {
  srand(time(NULL));
  /* puts("Nebula initalised."); */
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
           STRING = 128, CONS = 129, FUNCTION = 130, ARRAY = 131,
           POINTER = 132, VECTOR = 133, HASH_MAP = 134,
           KEYWORD = 135};
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

  retval->type = type;

  // We have to allocate the actual value on the heap, as LLVM stores
  // things on the stack only, so we'd lose our boxed value when
  // returning from a function. Nil is a special case, as value is
  // empty, and (usually) a null pointer, so we can skip this step.
  // Non-primitive values (type >= 128) can also safely be assigned
  // over, as they're only pointers anyway.
  if ((NIL != type) && (type < STRING)) {
    union Primitive* val = malloc(sizeof(union Primitive));
    if (NULL == val) {
      exit(ENOMEM);
    }
    *val = *value;
    retval->value = val;
  } else if ((STRING == type) || (KEYWORD == type)) {
    // Strings & keywords get special treatment.
    char* copy = malloc(sizeof(char) * (1 + strlen((char*)value)));
    if (NULL == copy) {
      exit(ENOMEM);
    }
    strcpy(copy, (char*)value);
    retval->value = (union Primitive*)copy;
  } else {
    retval->value = value;
  }

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
    return true;
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
    case ARRAY:
      printf("<type: array>");
      break;
    case POINTER:
      printf("<type: pointer>");
      break;
    case VECTOR:
      printf("<type: vector>");
      break;
    case HASH_MAP:
      printf("<type: hashmap>");
      break;
    case KEYWORD:
      printf("<type: keyword>");
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
    struct Value* head = cdr(value);
    while (NIL != head->type) {
      printf(" ");
      print_value(car(head));
      head = cdr(head);
    }
    printf(")");
    break;
  case FUNCTION:
    printf("<function: %s>", ((struct Function*)value->value->ptr)->name);
    break;
  case ARRAY:
    printf("[");
    /* print_value(value->value->i); */
    printf("]");
    break;
  case POINTER:
    printf("<ptr: %X>", (uint32_t)value->value->ptr);
    break;
  case VECTOR:
    printf("<vector: %X>", (uint32_t)value->value);
    break;
  case HASH_MAP:
    printf("<hash-map: %X>", (uint32_t)value->value);
    break;
  case KEYWORD:
    printf(":%s", (char *)(value->value));
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
    *(str+pos) = (char)((struct Cons*)head->value->ptr)->car->value->ptr;
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

struct Value* read_line() {
  char* buffer = NULL;
  size_t size;
  if (getline(&buffer, &size, stdin) == -1) {
    // XXX Error unhandled. Check feof(stdin) & ferror(stdin).
  }
  return make_value(STRING, (union Primitive*)buffer);
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

/* Arrays */

// Takes a cons list and creates an array.
struct Value* array(const int t, const struct Value* l) {
  size_t len = 0;
  const struct Value* head = l;
  // Walk once to get the length.
  while (NIL != head->value) {
    ++len;
    head = ((struct Cons*)head->value->ptr)->cdr;
  }
  head = l;
  // XXX Currently only does pointers.
  void** arr = malloc(len * sizeof(void*));
  if (NULL == arr) {
    exit(ENOMEM);
  }
  for (size_t pos = 0; pos < len; ++pos) {
    arr[pos] = ((struct Cons*)head->value->ptr)->car->value;
    head = ((struct Cons*)head->value->ptr)->cdr;
  }
  struct Value* rv = make_value(ARRAY, (void*)arr);
  return rv;
}

/* Pointers */

struct Value* make_pointer() {
  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->ptr = NULL;
  return make_value(POINTER, u);
}

void* deref(struct Value* val) {
  return val->value->ptr;
}

void call(void* fptr) {
  printf("%d\n", ((int (*)(int, int))fptr)(38, 4));
}

/* Utility */

// djb2 string hash function.
unsigned long hash(unsigned char* str) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}

/* Testing */

struct Value* random_bool() {
  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->b = rand() % 2;
  return make_value(BOOL, u);
}

struct Value* call_func(void* fn_ptr, int x, int y) {
  int result = ((int (*)(int, int))fn_ptr)(x, y);
  union Primitive* u = calloc(1, sizeof(union Primitive));
  u->i = result;
  return make_value(INT, u);
}

void debug_value(struct Value* val) {
  printf("[DEBUG] value type: %u\n", value_type(val));
  printf("[DEBUG] value pointer: base 10: %u; base 16: %X\n", (unsigned int)val, (unsigned int)val);
  union Primitive* u = val->value;
  printf("[DEBUG] value primitive: base 10: %u; base 16: %X\n", (unsigned int)u, (unsigned int)u);
  printf("[DEBUG] print value: ");
  print_value(val);
  printf("\n");
  /* void* ptr = val->value->ptr; */
  /* printf("[DEBUG] value value: base 10: %u; base 16: %X\n", (unsigned int)ptr, (unsigned int)ptr); */
}
