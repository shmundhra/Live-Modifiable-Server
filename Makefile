CC = g++ -std=c++17
.DEFAULT_GOAL := Menu
wrapper = wrapper_concurrent.cpp
server = server.cpp
client = client.cpp

Menu	:
	@echo "Welcome to Live Modifiable Server Makefile :: Default CC=$(CC) "
	@echo ""
	@echo "CHOOSE APPROPRIATE COMMAND : "
	@echo "All [CC=<CompilerVersion>] - Creates Default Object Files for All"
	@echo "Header  [CC=<CompilerVersion>] - Creates Header Executable \"livemodifiable.o\""
	@echo "Library [CC=<CompilerVersion>] - Creates Static Library \"liblms.a\""
	@echo "Wrapper  [CC=<CompilerVersion>] [wrapper=<SourceFile.cpp>] - Creates \"lm_server.o\""
	@echo "Server [CC=<CompilerVersion>] [server=<SourceFile.cpp>] - Creates \"server.o\""
	@echo "Client [CC=<CompilerVersion>] [client=<SourceFile.cpp>] - Creates \"client.o\""
	@echo "clean   - Removes All files created in this Makefile and Clears Screen"
	@echo "remove  - Removes All files created in this Makefile"
	@echo ""

All	:	Library Wrapper Server Client
	@echo "All Set for Action"

Wrapper		: LM_Server
	@echo ""
	@echo ">>\"./lm_server.o ./server.o \" TO START LM_Server Wrapper over server.o "
	@echo ""

LM_Server 	:	liblms.a
	$(CC) -o lm_server.o $(wrapper) -L. -llms

Server  :
	$(CC) -o server.o $(server) -L. -llms

Client  :
	$(CC) -o client.o $(client) -L. -llms

Library :	liblms.a
	@echo ""
	@echo ">>\"CC -o <lm_server>.o <wrapper>.cpp -L. -llms \" TO COMPILE A PROGRAM"
	@echo ""

liblms.a	:	livemodifiable.o
	ar rcs liblms.a livemodifiable.o

Header	:	livemodifiable.o
	@echo ""
	@echo ">>\"ar rcs lib<lms>.a livemodifiable.o\" TO CREATE STATIC LIBRARY"
	@echo ""

livemodifiable.o	:	livemodifiable.cpp	livemodifiable.h
	$(CC) livemodifiable.h
	$(CC) -c -o livemodifiable.o livemodifiable.cpp

clean	:
	rm -f server.o client.o lm_server.o livemodifiable.o livemodifiable.h.gch liblms.a
	clear

remove	:
	rm -f server.o client.o lm_server.o livemodifiable.o livemodifiable.h.gch liblms.a
