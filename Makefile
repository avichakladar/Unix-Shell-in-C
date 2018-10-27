all: dash.exe

dash.exe: dash.o
         gcc -o dash.exe dash.o

dash.o: dash.c
         gcc -g -c dash.c -o dash.o

clean:
         rm dash.o dash.exe