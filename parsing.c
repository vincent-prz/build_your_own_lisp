#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

/* Declare New lval Struct */
typedef struct {
  int type;
  long num;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval;

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create a new number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Create a new error type lval */
lval* lval_err(char* x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(x) + 1);
  strcpy(v->err, x);
  return v;
}

/* Create a new symbol lval */
lval* lval_sym(char* x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(x) + 1);
  strcpy(v->sym, x);
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

/* add expr to sexpr */
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
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

//lval* eval_op(lval* xval, char* op, lval* yval) {
//  if (xval->type == LVAL_ERR) {
//    return xval;
//  }
//  if (yval->type == LVAL_ERR) {
//    return yval;
//  }
//
//  long x = xval.num;
//  long y = yval.num;
//
//  if (strcmp(op, "+") == 0) { return lval_num(x + y); }
//  if (strcmp(op, "-") == 0) { return lval_num(x - y); }
//  if (strcmp(op, "*") == 0) { return lval_num(x * y); }
//  if (strcmp(op, "/") == 0) {
//    if (y) {
//      return lval_num(x / y);
//    } else {
//      return lval_err(LERR_DIV_ZERO);
//    }
//  }
//  if (strcmp(op, "%") == 0) { return lval_num(x % y); }
//  if (strcmp(op, "^") == 0) { return lval_num((int) pow((double) x, y)); }
//  // if (strcmp(op, "m") == 0) { if (x <= y) { return x; } else { return y; }; }
//
//  // why is this useful ? Won't we have a parsing error in this case ?
//  return lval_err(LERR_BAD_OP);
//}
//
lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
  }
  if(strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }
  if(strcmp(t->tag, ">") == 0|| strstr(t->tag, "sexpr")) {
    lval* v = lval_sexpr();
    for (int i = 0; i < t->children_num; i++) {
      mpc_ast_t* child = t->children[i];
      if (strcmp(child->tag, "regex") == 0 || strcmp(child->contents, "(") == 0 || strcmp(child->contents, ")") == 0) {
	continue;
      }
      v = lval_add(v, lval_read(child));
    }
    return v;
  }
  return NULL;

 // char* op = t->children[1]->contents;

 // lval* result = eval(t->children[2]);
 // int i = 3;
 // while (strstr(t->children[i]->tag, "expr")) {
 //   result = eval_op(result, op, eval(t->children[i]));
 //   i++;
 // }

 // return result;
}

int main(int argc, char** argv) {
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* SExpr     = mpc_new("sexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      symbol   : '+' | '-' | '*' | '/' | '%' | '^' ; \
      sexpr    : '(' <expr>* ')' ;  \
      expr     : <number> | <symbol> | <sexpr> ;  \
      lispy    : /^/ <expr>* /$/ ;             \
    ",
    Number, Symbol, SExpr, Expr, Lispy);


  /* Print Version and Exit Information */
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");

  /* In a never ending loop */
  while (1) {

    /* Output our prompt and get input */
    char* input = readline("lispy> ");

    /* Add input to history */
    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t result;
    if (mpc_parse("<stdin>", input, Lispy, &result)) {
      mpc_ast_print(result.output);
      lval* v = lval_read(result.output);
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
  mpc_cleanup(5, Number, Symbol, SExpr, Expr, Lispy);
  return 0;
}
