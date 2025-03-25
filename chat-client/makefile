CC = cc
CFLAGS = -Wall -Iinc -I../Common/inc
LDFLAGS = -lncurses

SRC = src/client.c
OBJ = obj/client.o
BIN = bin/chat-client

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/*.o $(BIN)

