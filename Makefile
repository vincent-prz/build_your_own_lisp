lispy : lispy.c mpc.c mpc.h
	cc -std=c99 -Wall -g lispy.c mpc.c -ledit -lm -o lispy && ./lispy < test_input.txt > tmp.txt
