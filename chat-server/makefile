# Name of the executable
programName = chat-server

# Default target: build the executable
all: bin/$(programName)

# Link object file to create executable and set its permissions
bin/$(programName): obj/chat-server.o
	@mkdir -p bin
	cc obj/chat-server.o -o bin/$(programName)
	chmod 771 bin/$(programName)

# Compile source file into object file; depends on header file
obj/chat-server.o: src/chat-server.c inc/chat-server.h
	@mkdir -p obj
	cc -c src/chat-server.c -o obj/chat-server.o

# Clean up object file and executable
clean:
	rm -f obj/*.o
	rm -f bin/$(programName)
