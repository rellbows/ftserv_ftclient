README for 'ftclient'/'ftserver'

Description:

Simple file transfer that allows ftclient to see the working directory of ftserver and request files from that server.

Compile Instructions:

'ftserver': Run 'make' in a machines command line.
'ftclient': N/A - python scripts do not need to be compiled first.

Execution Instructions:

'ftserver': Type './ftserver [host control port number]' into the command line.

'ftclient': Type 'python3 ftclient.py [host name] [host control port number] [-g or -l] [filename requested (if -g option)] [host data port number]' into the command line.

Usage Instructions:

'chatclient': 
1. When calling script from command line, specify the below.
-the host name the server is running on
-the port number the server will be listening on
- -g (for getting a file) or -l (for getting the working directory of server) options
-the filename requested (if the -g option is used)
-the port number the server will send back the data on (working director or file).

'chatserv':
1. When calling script from command line, specify the below.
-the port number the server will be listening on for instructions from the client
2. To end script, enter '⌘C' (command button + 'c' button).

Testing Details:
Scripts were tested on a MacBook Pro running macOS Sierra ver. 10.12.6.

Bugs/TODOS:
1. Need to complete more testing to ensure that text files are being sent accurately.
2. Make server multi-threaded.
3. Implement username/password access to server.
4. Allow client to change directory on the server.
5. Transfer files additional to text files (e.g. binary files).
