# TCP/IP chat implementation

This repository contains an TCP/IP server and client implementation
It contains 3 programs:
    -Server
    -Client
    -send_message

# How to use it
  1. Compile each source file.
  2. Run the **Server** passing it the **port** number as parameter
    > ./tcpserver.elf "port"

  4. Run the **Client** passing it the **IP**, **port** and the **name** as parameters
  
  4. Exchange messages in the chat:
    - each client has a terminal to see the receives messages;
    - to send a message, the *send_message* is used.
        ex: if you want so send the message **"what's up meus putos"**, you do: 
            >./send_message "what's up meus putos"
  5. Repeat
