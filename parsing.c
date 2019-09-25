#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include "mpc.h"

long eval_op(long x, char* op, long y) {
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  if (strcmp(op, "%") == 0) { return x % y; }
  if (strcmp(op, "^") == 0) { return (int) pow((double) x, y); }
  // if (strcmp(op, "m") == 0) { if (x <= y) { return x; } else { return y; }; }
}

long eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  char* op = t->children[1]->contents;

  long result = eval(t->children[2]);
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
      /* On Success Print the AST */
      printf("%li\n", eval(result.output));
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
