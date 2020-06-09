# Live-Modifiable-Server
A Wrapper Server which can pause a connection, allow modification to Server Code and resume from same point.

# Instructions

## Credentials
	Go to livemodifiable.h and change the following according to need
	IP_ADDR   - The IP Address of your Server
	PORT	  - The Port of your Server
	EMULATING - Whether or not you are emulating on a platform (MiniNAM)

## Registration of Backup Nodes at Server
Initially Register Backup Nodes at the Server. When you are done wait for a timeout to occur or breakout using a Ctrl+D (EOF) at the STDIN.

## Connection of Clients
Connect Appropriate clients to the Server depending on the functoinality you want to execute - GET or PUT.

## Code Modification
Generate a SIGINT using Ctrl+C in the terminal to pause the server. Make sure you send the SIGINT to the whole process group rather than just the wrapper server (in the terminal this happens automatically). 
Once the modified executable is ready, generate another SIGINT to resume the server and the existing connections. During this pause time, the server shall still be accepting connections, they shall just be queued.

# How to Run
	Compile the Wrapper(Concurrent) using the necessary Make Command
	Compile the Server and Clients using the necessary Make Commands
	Run the Wrapper Executable with the Server Executable as argv[1]
	Run the BACKUP_Node Executables with the ServerIP as argv[1]
	Run the GET_Client Executables to Connect to the Server with Filename argv[1] to GET, and ServerIP as argv[2]
	Run the PUT_Client Executables to Connect to the Server with Filename argv[1] to PUT to Server and Backup, and ServerIP as argv[2]

