# Name of the executable
programName = chat-client

# Default target: build the executable
all: bin/$(programName)

# Link object file to create executable and set its permissions
bin/$(programName): obj/chat-client.o
	@mkdir -p bin
	cc obj/chat-client.o -o bin/$(programName) -lncurses
	chmod 771 bin/$(programName)

# Compile source file into object file; depends on header file
obj/chat-client.o: src/chat-client.c inc/chat-client.h ../Common/inc/common.h
	@mkdir -p obj
	cc -c src/chat-client.c -o obj/chat-client.o

# Clean up object file and executable
clean:
	rm -f obj/*.o
	rm -f bin/$(programName)