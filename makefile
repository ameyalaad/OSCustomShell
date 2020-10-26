all: main.c
	gcc -Wall -o main main.c
	./main

clean: 
	rm main command.log

nothing:
