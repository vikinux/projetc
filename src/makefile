src = main.c

surictac : main.o
	gcc -Wall -Wextra  -l sqlite3 main.o -o surictac

main.o : main.c
	gcc -Wall -g -Wextra  -l sqlite3 -c main.c -o main.o

