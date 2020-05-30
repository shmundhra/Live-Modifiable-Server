# Live-Modifiable-Server
A Wrapper Server which can pause a connection, allow modification to Server Code and resume from same point

# Instructions
Generate a SIGINT using Ctrl+C to pause the server. Once the modified executable is ready, generate another SIGINT to resume the server and the existing connections. During this pause time, the server shall still be accepting connections, they shall just be queued.