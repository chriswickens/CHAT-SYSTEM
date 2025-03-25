all:
	$(MAKE) -C chat-client
	$(MAKE) -C chat-server

clean:
	$(MAKE) -C chat-client clean
	$(MAKE) -C chat-server clean

