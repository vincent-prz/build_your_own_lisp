#!/bin/bash
cc -std=c99 -Wall -g lispy.c mpc.c -ledit -lm -o lispy && ./lispy < test_input.txt > tmp.txt

# TODO: try to remove the first 3 lines of the comparison, with `tail -n +4 <file>
diff tmp.txt test_expected.txt
if [ $? -ne 0 ]
then
  echo "test failed"
  rm tmp.txt
  exit 1
else
  echo "test successful"
  rm tmp.txt
  exit 0
fi
