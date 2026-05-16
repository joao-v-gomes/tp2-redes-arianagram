CC := gcc
FLAGS := -I include/ -Wall 
# LFLAGS := -lpqxx -lpq
BUILDDIR := build
SRCDIR := src
# TESTDIR := testes
# TARGET := main.out
# LIBSDIR := -L/usr/local/lib -L/usr/lib/x86_64-linux-gnu/

all: clean client server

client:
	$(CC) $(FLAGS) $(SRCDIR)/client.c $(SRCDIR)/util.c -o client

server:
	$(CC) $(FLAGS) $(SRCDIR)/server.c $(SRCDIR)/util.c -o server


# all: clean main
# 	$(CC) $(FLAGS) $(BUILDDIR)/*.o -o $(TARGET) $(LIBSDIR) $(LFLAGS)

# main: menu usuarios cliente bibliotecario estante prateleira livro
# 	$(CC) $(FLAGS) -c $(SRCDIR)/main.cpp -o $(BUILDDIR)/main.o

clean:
	rm -rf build/*.o
	rm -rf client server