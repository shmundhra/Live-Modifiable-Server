CC = g++ -std=c++17
.DEFAULT_GOAL := Menu
wrapper = wrapper_concurrent.cpp
server = server.cpp
get_client = get_client.cpp
put_client = put_client.cpp
bup_client = backup_client.cpp

Menu	:
	@echo "Welcome to Live Modifiable Server Makefile :: Default CC=$(CC) "
	@echo ""
	@echo "CHOOSE APPROPRIATE COMMAND : "
	@echo "All [CC=<CompilerVersion>] - Creates Default Object Files for All"
	@echo "Header  [CC=<CompilerVersion>] - Creates Header Executable \"livemodifiable.o\""
	@echo "Library [CC=<CompilerVersion>] - Creates Static Library \"liblms.a\""
	@echo "Wrapper  [CC=<CompilerVersion>] [wrapper=<SourceFile.cpp>] - Creates \"lm_server.o\""
	@echo "Server [CC=<CompilerVersion>] [server=<SourceFile.cpp>] - Creates \"server.o\""
	@echo "GetClient [CC=<CompilerVersion>] [get_client=<SourceFile.cpp>] - Creates \"get_client.o\""
	@echo "PutClient [CC=<CompilerVersion>] [put_client=<SourceFile.cpp>] - Creates \"put_client.o\""
	@echo "BackupClient [CC=<CompilerVersion>] [bup_client=<SourceFile.cpp>] - Creates \"bup_client.o\""
	@echo "clean   - Removes All files created in this Makefile and Clears Screen"
	@echo "remove  - Removes All files created in this Makefile"
	@echo ""

All	:	Library Wrapper Server Client
	@echo ""
	@echo "ALL SET FOR ACTION !!!"
	@echo ""

Wrapper		: LM_Server
	@echo "BINARIES CREATED >> \"./lm_server.o ./server.o\" TO START LM_Server Wrapper over server.o "
	@echo ""

LM_Server 	:	liblms.a
	$(CC) -o lm_server.o $(wrapper) -L. -llms

Server	:
	$(CC) -o server.o $(server) -L. -llms

Client	:	GetClient	PutClient	BackupClient

GetClient  :
	$(CC) -o get_client.o $(get_client) -L. -llms

PutClient  :
	$(CC) -o put_client.o $(put_client) -L. -llms

BackupClient  :
	$(CC) -o bup_client.o $(bup_client) -L. -llms

Library :	liblms.a
	@echo "LIBRARY CREATED >> \"CC -o <lm_server>.o <wrapper>.cpp -L. -llms\" TO COMPILE A PROGRAM"
	@echo ""

liblms.a	:	livemodifiable.o
	ar rcs liblms.a livemodifiable.o

Header	:	livemodifiable.o
	@echo "HEADER CREATED >> \"\ar rcs lib<lms>.a livemodifiable.o\" TO CREATE STATIC LIBRARY"
	@echo ""

livemodifiable.o	:	livemodifiable.cpp	livemodifiable.h
	$(CC) livemodifiable.h
	$(CC) -c -o livemodifiable.o livemodifiable.cpp

clean	:
	rm -f server.o get_client.o put_client.o bup_client.o lm_server.o livemodifiable.o livemodifiable.h.gch liblms.a
	clear

remove	:
	rm -f server.o get_client.o put_client.o bup_client.o lm_server.o livemodifiable.o livemodifiable.h.gch liblms.a
