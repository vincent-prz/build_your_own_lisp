#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

/* Declare New lval Struct */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR };

/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create a new number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* Create a new error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* Print an "lval" */
void lval_print(lval v) {
  switch (v.type) {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM: printf("%li", v.num); break;

    /* In the case the type is an error */
    case LVAL_ERR:
      /* Check what type of error it is and print it */
      if (v.err == LERR_DIV_ZERO) {
        printf("Error: Division By Zero!");
      }
      if (v.err == LERR_BAD_OP)   {
        printf("Error: Invalid Operator!");
      }
      if (v.err == LERR_BAD_NUM)  {
        printf("Error: Invalid Number!");
      }
    break;
  }
}

/* Print an "lval" followed by a newline */
void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_op(lval xval, char* op, lval yval) {
  if (xval.type == LVAL_ERR) {
    return xval;
  }
  if (yval.type == LVAL_ERR) {
    return yval;
  }

  long x = xval.num;
  long y = yval.num;

  if (strcmp(op, "+") == 0) { return lval_num(x + y); }
  if (strcmp(op, "-") == 0) { return lval_num(x - y); }
  if (strcmp(op, "*") == 0) { return lval_num(x * y); }
  if (strcmp(op, "/") == 0) {
    if (y) {
      return lval_num(x / y);
    } else {
      return lval_err(LERR_DIV_ZERO);
    }
  }
  if (strcmp(op, "%") == 0) { return lval_num(x % y); }
  if (strcmp(op, "^") == 0) { return lval_num((int) pow((double) x, y)); }
  // if (strcmp(op, "m") == 0) { if (x <= y) { return x; } else { return y; }; }

  // why is this useful ? Won't we have a parsing error in this case ?
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  char* op = t->children[1]->contents;

  lval result = eval(t->children[2]);
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    result = eval_op(result, op, eval(t->children[i]));
    i++;
  }

  return result;
}

int main(int argc, char** argv) {
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      operator : '+' | '-' | '*' | '/' | '%' | '^' ; \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, Lispy);


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
      lval_println(eval(result.output));
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
  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}
