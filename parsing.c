#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

/* Declare New lval Struct */
typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval;

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* Create a new number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  v->count = 0;
  return v;
}

/* Create a new error type lval */
lval* lval_err(char* x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(x) + 1);
  strcpy(v->err, x);
  v->count = 0;
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

lval* lval_add(lval* parent, lval* child) {
  parent->count++;
  parent->cell = realloc(parent->cell, sizeof(lval*) * parent->count);
  parent->cell[parent->count-1] = child;
  return parent;
}

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

/* vx and vy must be of type LVAL_NUM */
lval* eval_op(lval* vx, char* op, lval* vy) {
  long x = vx->num;
  long y = vy->num;
  if (strcmp(op, "+") == 0) { return lval_num(x + y); }
  if (strcmp(op, "-") == 0) { return lval_num(x - y); }
  if (strcmp(op, "*") == 0) { return lval_num(x * y); }
  if (strcmp(op, "/") == 0) {
    if (y) {
      return lval_num(x / y);
    } else {
      return lval_err("Division By Zero!");
    }
  }
  if (strcmp(op, "%") == 0) { return lval_num(x % y); }
  if (strcmp(op, "^") == 0) { return lval_num((int) pow((double) x, y)); }

  // why is this useful ? Won't we have a parsing error in this case ?
  return lval_err("Unknown operator");
}

lval* builtin_op(lval* a, char* op) {
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }
  lval* result = lval_pop(a, 0);
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    result->num = -result->num;
  }

  for (int i = 0; i < a->count; i++) {
    result = eval_op(result, op, a->cell[i]);
  }
  lval_del(a);
  return result;
}

lval* builtin_list(lval* a) {
  lval* result = lval_qexpr();
  while(a->count > 0) {
    lval_add(result, lval_pop(a, 0));
  }
  lval_del(a);
  return result;
}


lval* builtin_head(lval* a) {
  LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments!")
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect types!")
  LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}!")

  lval* qexpr = lval_take(a, 0);
  while (qexpr->count > 1) {
    lval_pop(qexpr, 1);
  }
  return qexpr;
}

lval* builtin_tail(lval* a) {
  LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments!")
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect types!")
  LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed {}!")

  lval* qexpr = lval_take(a, 0);
  lval* head = lval_pop(qexpr, 0);
  lval_del(head);
  return qexpr;
}

lval* builtin_join(lval* a) {
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

lval* lval_eval(lval* v);

lval* builtin_eval(lval* a) {
  LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments!")
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!")

  lval* qexpr = lval_pop(a, 0);
  lval* result = lval_sexpr();
  while(qexpr->count > 0) {
    lval_add(result, lval_pop(qexpr, 0));
  }
  lval_del(a);
  lval_del(qexpr);
  return lval_eval(result);
}

lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strstr("+-/*", func)) { return builtin_op(a, func); }
  lval_del(a);
  return lval_err("Unknown Function!");
}

lval* lval_eval_sexpr(lval* v) {
  /* Evaluate children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
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
  lval* op = lval_pop(v, 0);
  /* Check that it starts with an expression, and if not return an error */
  if (op->type != LVAL_SYM) {
    lval_del(op);
    lval_del(v);
    return lval_sym("S-expression does not start with symbol!");
  }
  lval* result = builtin(v, op->sym);
  lval_del(op);
  return result;
}

lval* lval_eval(lval* v) {
  /* Evaluate Sexpressions */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v;
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
      symbol : \"list\" | \"head\" | \"tail\"                \
	     | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
      sexpr    : '(' <expr>* ')' ;  \
      qexpr  : '{' <expr>* '}' ;                         \
      expr     : <number> | <symbol> | <sexpr> | <qexpr> ;  \
      lispy    : /^/ <expr>* /$/ ;             \
    ",
    Number, Symbol, SExpr, QExpr, Expr, Lispy);


  /* Print Version and Exit Information */
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");

  /* In a never ending loop */
  while (1) {

    /* Output our prompt and get input */
    char* input = readline("lispy> ");
    if (strcmp(input, "q") == 0) {
      free(input);
      break;
    }

    /* Add input to history */
    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t result;
    if (mpc_parse("<stdin>", input, Lispy, &result)) {
      // mpc_ast_print(result.output);
      lval* v = lval_eval(lval_read(result.output));
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
  return 0;
}
