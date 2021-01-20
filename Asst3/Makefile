CC = gcc
PREFLAGS = -Wall -o

all: greet generate

greet:
	@echo "Now running MAKEFILE..."

generate: Asst3.c
	@echo "Generating KKJserver.exe from Asst3.c..."
	$(CC) $(PREFLAGS) KKJserver Asst3.c

clean:
	@echo "Cleaning up..."
	rm -f KKJserver