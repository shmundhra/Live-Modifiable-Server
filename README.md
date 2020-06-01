# Live-Modifiable-Server
A Wrapper Server which can pause a connection, allow modification to Server Code and resume from same point

# Instructions
Generate a SIGINT using Ctrl+C to pause the server. Once the modified executable is ready, generate another SIGINT to resume the server and the existing connections. During this pause time, the server shall still be accepting connections, they shall just be queued.

# How to Run
	Compile the Wrapper(Concurrent) using the necessary Make Command
	Compile the Server and Client using the neccesary Make Commands
	Run the Wrapper Executable with the Server Executable as CommandLineArguement-2
	Run the Client Executables to Connect to the Server with Filename to GET and Server IP_Address as CommandLineArguements

