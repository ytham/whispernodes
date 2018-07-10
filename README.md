# Whisper Nodes
Yu Jiang Tham

### Building
This project is coded in C++. To build, run the following in the top level directory of this project:
1. cmake .
2. make

### Running
Individual nodes can be set up by running the following command:

> ./whisper_client --name [name] --port [port]

When a client is created, it will assign itself to a port. To create a node that is connected to another node, use the following command:

> ./whisper_client --name [name] --port [port] --bootnodes <comma_separated_list>

The clients put out silent initial discovery messages to create a neighbor map. When a message is sent to one node, it will forward it off to the others until the target is found.

> curl -X GET "localhost:4000/whisper?name=[target]&message=[message]"
