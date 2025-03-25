CC = cc
CFLAGS = -Wall -Iinc -I../Common/inc
LDFLAGS =

SRC = src/chat-server.c
OBJ = obj/chat-server.o
BIN = bin/chat-server

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/*.o $(BIN)

