# build_your_own_lisp

This repository contains the code I built while [this book](http://buildyourownlisp.com/). While this code is obviously very similar to the code which can be found in the book, it contains some minor differences since I wrote the code myself, instead of copying the samples from the book <sup>1</sup>. As a result, there currently are some bugs and memory leaks, which I need to fix :). My main goal was to educate myself on programming languages implementations.

## Usage

Just build the REPL via `make`, and launch the `lispy` executable.

## Features

The code currently covers roughly the language features described until chapter 12. Namely, its consists in a REPL which can do the following things:

### Arbitrarily nested arithmetic operations

```
lispy> + 1 2
3
lispy> * 4 (+ 2 3)
20
```

### Use Q Expressions (or lists)

```
lispy> {1 2}
{1 2}
lispy> head {1 2}
1
lispy> tail {1 2}
{2}
```

### Evaluate Q Expressions

In Lisp, code can be seen as data. To support this idea, there is the possibility to evaluate a list via the `eval` builtin:
```
lispy> eval {+ 1 2}
3
```

### Define variables

```
lispy> def {x} 2
()
lispy> x
2
lispy> def {x y} 1 2
()
lispy> + x y
3
```

### Define functions
```
lispy>\ {x} {+ x 1}
(\ {x} {+ x 1})
lispy>(\ {x} {+ x 1}) 2
3
lispy> def {add} (\ {x y} {+ x y})
()
lispy> add 4 5 
9
```
### Notes

[1]: this is not true for the `mpc.c` amd `mpc.h` files, which are given as a black box by the author, and hence have been copied.
