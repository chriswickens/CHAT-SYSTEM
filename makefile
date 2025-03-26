.PHONY: all clean

# The top-level "all" target calls the makefiles in the subdirectories.
all:
	$(MAKE) -C chat-client
	$(MAKE) -C chat-server
# Uncomment the next line for Common
# $(MAKE) -C Common

# The top-level "clean" target cleans all subdirectories.
clean:
	$(MAKE) -C chat-client clean
	$(MAKE) -C chat-server clean
# Uncomment the next line for common
# $(MAKE) -C Common clean
