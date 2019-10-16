#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }

// TODO: improve error messages everywhere, like in `builtin_head`

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare New lval Struct */
typedef struct lval {
  int type;

  /* Basic */
  long num;
  char* err;
  char* sym;

  /* Function */
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;

  /* Expression */
  int count;
  lval** cell;
} lval;

struct lenv {
  lenv* par;
  int count;
  char** syms;
  lval** vals;
};

/* Create a new number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  v->count = 0;
  return v;
}

/* Create a new error type lval */
lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  /* Create a va list and initialize it */
  va_list va;
  va_start(va, fmt);

  /* Allocate 512 bytes of space */
  v->err = malloc(512);

  /* printf the error string with a maximum of 511 characters */
  vsnprintf(v->err, 511, fmt, va);

  /* Reallocate to number of bytes actually used */
  v->err = realloc(v->err, strlen(v->err)+1);

  /* Cleanup our va list */
  va_end(va);

  return v;
}

/* Create a new symbol lval */
lval* lval_sym(char* x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(x) + 1);
  strcpy(v->sym, x);
  v->count = 0;
  return v;
}

/* Create a new sexpr lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}


/* Create a new qexpr lval */
lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_builtin(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->count = 0;
  v->builtin = func;
  return v;
}

lenv* lenv_new(void);

lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  /* Set Builtin to Null */
  v->builtin = NULL;

  /* Build new environment */
  v->env = lenv_new();

  /* Set Formals and Body */
  v->formals = formals;
  v->body = body;
  return v;
}

char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}

lval* lval_add(lval* parent, lval* child) {
  parent->count++;
  parent->cell = realloc(parent->cell, sizeof(lval*) * parent->count);
  parent->cell[parent->count-1] = child;
  return parent;
}

void lenv_del(lenv* e);

void lval_del(lval* v) {
  switch(v->type) {
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      for (int i = 0; i < v->count; i++) {
	lval_del(v->cell[i]);
      }
      free(v->cell);
      break;
    case LVAL_FUN:
      if (v->builtin == NULL) {
	lenv_del(v->env);
	lval_del(v->formals);
	lval_del(v->body);
      }
      break;
  }
  free(v);
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {

    /* Print Value contained within */
    lval_print(v->cell[i]);

    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    case LVAL_FUN:
		     if (v->builtin) {
		       printf("<builtin>");
		     } else {
		       printf("(\\ "); lval_print(v->formals);
		       putchar(' '); lval_print(v->body); putchar(')');
		     }
		     break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
  }
  if(strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }
  lval* v = NULL;
  if(strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr")) {
    v = lval_sexpr();
  }
  if(strstr(t->tag, "qexpr")) {
    v = lval_qexpr();
  }
  for (int i = 0; i < t->children_num; i++) {
    mpc_ast_t* child = t->children[i];
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    v = lval_add(v, lval_read(child));
  }
  return v;
}

lenv* lenv_copy(lenv* e);

lval* lval_copy(lval* v) {

  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {

    /* Copy Functions and Numbers Directly */
    case LVAL_FUN:
      if (v->builtin != NULL) {
	x->builtin = v->builtin;
      }
      else {
	x->env = lenv_copy(v->env);
	x->formals = lval_copy(v->formals);
	x->body = lval_copy(v->body);
      }
      break;
    case LVAL_NUM: x->num = v->num; break;

    /* Copy Strings using malloc and strcpy */
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;

    /* Copy Lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }

  return x;
}

lenv* lenv_copy(lenv* e) {
  lenv* x = malloc(sizeof(lenv));
  x->par = e->par;
  x->count = e->count;
  x->syms = malloc(sizeof(char *) * x->count);
  x->vals = malloc(sizeof(lval *) * x->count);
  for (int i = 0; i < x->count; i++) {
    x->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(x->syms[i], e->syms[i]);
    x->vals[i] = lval_copy(e->vals[i]);
  }
  return x;
}

/* removes values from SExpr */
lval* lval_pop(lval* v, int i) {
  lval* result = v->cell[i];

  memmove(&v->cell[i], &v->cell[i+1],
  sizeof(lval*) * (v->count-i-1));

  v->count--;

  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return result;
}

/* take value from SExpr, and free SExpr */
lval* lval_take(lval* v, int i) {
  lval* result = lval_pop(v, i);
  lval_del(v);
  return result;
}

lval* builtin_plus(lenv *e, lval *v) {
  for (int i = 0; i < v->count; i++) {
    LASSERT(v->cell[i], v->cell[i]->type == LVAL_NUM, "Cannot operate on non-number!")
  }
  long current_val = 0;
  while(v->count > 0) {
    lval* x = lval_pop(v, 0);
    current_val += x->num;
    lval_del(x);
  }
  lval_del(v);
  return lval_num(current_val);
}

lval* builtin_minus(lenv *e, lval *v) {
  for (int i = 0; i < v->count; i++) {
    LASSERT(v->cell[i], v->cell[i]->type == LVAL_NUM, "Cannot operate on non-number!")
  }
  long current_val;
  if (v->count == 1) {
    lval* x = lval_pop(v, 0);
    current_val = -x->num;
    lval_del(x);
  }
  else {
    lval* x = lval_pop(v, 0);
    current_val = x->num;
    lval_del(x);
    while(v->count > 0) {
      lval* x = lval_pop(v, 0);
      current_val -= x->num;
      lval_del(x);
    }
  }
  lval_del(v);
  return lval_num(current_val);
}

lval* builtin_times(lenv *e, lval *v) {
  for (int i = 0; i < v->count; i++) {
    LASSERT(v->cell[i], v->cell[i]->type == LVAL_NUM, "Cannot operate on non-number!")
  }
  long current_val = 1;
  while(v->count > 0) {
    lval* x = lval_pop(v, 0);
    current_val *= x->num;
    lval_del(x);
  }
  lval_del(v);
  return lval_num(current_val);
}

lval* builtin_div(lenv *e, lval *v) {
  for (int i = 0; i < v->count; i++) {
    LASSERT(v->cell[i], v->cell[i]->type == LVAL_NUM, "Cannot operate on non-number!")
  }
  lval* x = lval_pop(v, 0);
  long current_val = x->num;
  while(v->count > 0) {
    x = lval_pop(v, 0);
    if (x->num == 0) {
      lval_del(x);
      lval_del(v);
      return lval_err("Division By Zero!");
    }
    current_val /= x->num;
    lval_del(x);
  }
  lval_del(v);
  return lval_num(current_val);
}

lval* builtin_list(lenv* e,lval* a) {
  lval* result = lval_qexpr();
  while(a->count > 0) {
    lval_add(result, lval_pop(a, 0));
  }
  lval_del(a);
  return result;
}

lval* builtin_head(lenv* e, lval* a) {
  LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments. Got %i, Expected %i.", a->count, 1)
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect types. Got %s, Expected %s.", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR))
  LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}!")

  lval* qexpr = lval_take(a, 0);
  while (qexpr->count > 1) {
    lval_pop(qexpr, 1);
  }
  return qexpr;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments. Got %i, Expected %i.", a->count, 1)
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect types. Got %s, Expected %s.", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR))
  LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed {}!")

  lval* qexpr = lval_take(a, 0);
  lval* head = lval_pop(qexpr, 0);
  lval_del(head);
  return qexpr;
}

lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed incorrect type!");
  }
  lval* result = lval_qexpr();
  while(a->count > 0) {
    lval* qexpr = lval_pop(a, 0);
    while(qexpr->count > 0) {
      lval_add(result, lval_pop(qexpr, 0));
    }
    lval_del(qexpr);
  }
  lval_del(a);
  return result;
}
lval* lenv_get(lenv* e, lval* k);
void lenv_put(lenv* e, lval* k, lval* v);
void lenv_def(lenv* e, lval* k, lval* v);

lval* builtin_var(lenv* e, lval* a, char* func) {
  LASSERT(a, (a->count >= 2), "Function 'def' should be supplied at least 2 arguments")
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'def' should be supplied a QExpr as a first argument")
  LASSERT(a, (a->cell[0]->count == a->count - 1), "Function 'def' should be supplied as much variable names as values to assign")
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM), "Function 'def' should be supplied a QExpr with symbols as its children")
  }
  for (int i = 0; i < a->cell[0]->count; i++) {
    if (strcmp(func, "def") == 0) {
      lenv_def(e, a->cell[0]->cell[i], a->cell[i + 1]);
    } else {
      lenv_put(e, a->cell[0]->cell[i], a->cell[i + 1]);
    }
  }
  lval_del(a);
  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

lval* builtin_lambda(lenv* e, lval* a) {
  LASSERT(a, (a->count == 2), "Lambda passed incorrect number of arguments. Got %i, Expected %i.", a->count, 2)
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Lambda definition got incorrect argument type at position %i. Got %s, Expected %s.", i, ltype_name(a->cell[i]->type), ltype_name(LVAL_QEXPR)) }
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, a->cell[0]->cell[i]->type == LVAL_SYM, "Lambda definition got incorrect argument type for formal argument %i. Got %s, Expected %s.", i, ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM))
  }
  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);
  return lval_lambda(formals, body);
}

lval* lenv_get(lenv* e, lval* k) {

  lenv* current_e = e;
  while (current_e) {
    /* Iterate over all items in environment */
    for (int i = 0; i < e->count; i++) {
      /* Check if the stored string matches the symbol string */
      /* If it does, return a copy of the value */
      if (strcmp(e->syms[i], k->sym) == 0) {
	return lval_copy(e->vals[i]);
      }
    }
    current_e = current_e->par;
  }
  /* If no symbol found return error */
  return lval_err("Unbound Symbol %s", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  e->syms = realloc(e->syms, sizeof(char *) * e->count);
  e->vals = realloc(e->vals, sizeof(lval *) * e->count);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
  e->vals[e->count - 1] = lval_copy(v);
}

void lenv_def(lenv* e, lval* k, lval* v) {
  /* Iterate till e has no parent */
  while (e->par) { e = e->par; }
  /* Put value in e */
  lenv_put(e, k, v);
}

lval* lval_eval(lenv* e, lval* v);

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments!")
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!")

  lval* qexpr = lval_pop(a, 0);
  lval* result = lval_sexpr();
  while(qexpr->count > 0) {
    lval_add(result, lval_pop(qexpr, 0));
  }
  lval_del(a);
  lval_del(qexpr);
  return lval_eval(e, result);
}

void lenv_add_single_builtin(lenv* e, lval* k, lval* v) {
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  /* Qexpr builtins */
  lenv_add_single_builtin(e, lval_sym("list"), lval_builtin(builtin_list));
  lenv_add_single_builtin(e, lval_sym("head"), lval_builtin(builtin_head));
  lenv_add_single_builtin(e, lval_sym("tail"), lval_builtin(builtin_tail));
  lenv_add_single_builtin(e, lval_sym("join"), lval_builtin(builtin_join));
  lenv_add_single_builtin(e, lval_sym("eval"), lval_builtin(builtin_eval));
  /* Math builtins */
  lenv_add_single_builtin(e, lval_sym("+"), lval_builtin(builtin_plus));
  lenv_add_single_builtin(e, lval_sym("-"), lval_builtin(builtin_minus));
  lenv_add_single_builtin(e, lval_sym("*"), lval_builtin(builtin_times));
  lenv_add_single_builtin(e, lval_sym("/"), lval_builtin(builtin_div));

  /* Other */
  lenv_add_single_builtin(e, lval_sym("def"), lval_builtin(builtin_def));
  lenv_add_single_builtin(e, lval_sym("="), lval_builtin(builtin_put));
  lenv_add_single_builtin(e, lval_sym("\\"), lval_builtin(builtin_lambda));
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
  /* Evaluate children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  if (v->count == 0) {
    return v;
  }

  if (v->count == 1) {
      return lval_take(v, 0);
  }
  /* at this point, we have a valid expression with more than one expression */
  lval* f = lval_pop(v, 0);
  /* Check that it starts with an expression, and if not return an error */
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err("first element is not a function");
  }
  lval* result = f->builtin(e, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  /* Evaluate Sexpressions */
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
  return v;
}

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

int main(int argc, char** argv) {
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* SExpr     = mpc_new("sexpr");
  mpc_parser_t* QExpr     = mpc_new("qexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
      sexpr    : '(' <expr>* ')' ;  \
      qexpr  : '{' <expr>* '}' ;                         \
      expr     : <number> | <symbol> | <sexpr> | <qexpr> ;  \
      lispy    : /^/ <expr>* /$/ ;             \
    ",
    Number, Symbol, SExpr, QExpr, Expr, Lispy);


  /* Print Version and Exit Information */
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");

  /* Create environment */
  lenv* e = lenv_new();
  lenv_add_builtins(e);

  /* In a never ending loop */
  while (1) {

    /* Output our prompt and get input */
    char* input = readline("lispy> ");
    /* quit if given the letter 'q' */
    if (strcmp(input, "q") == 0) {
      free(input);
      break;
    }
    /* Comments TODO handle this properly in grammar */
    if (strlen(input) > 0 && strstr(input, "#")) {
      continue;
    }

    /* Add input to history */
    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t result;
    if (mpc_parse("<stdin>", input, Lispy, &result)) {
      // mpc_ast_print(result.output);
      lval* v = lval_eval(e, lval_read(result.output));
      lval_println(v);
      lval_del(v);
      mpc_ast_delete(result.output);
    } else {
      /* Otherwise Print the Error */
      mpc_err_print(result.error);
      mpc_err_delete(result.error);
    }

    /* Free retrieved input */
    free(input);

  }
  /* Undefine and Delete our Parsers */
  mpc_cleanup(6, Number, Symbol, SExpr, QExpr, Expr, Lispy);
  /* Delete environment */
  lenv_del(e);
  return 0;
}
