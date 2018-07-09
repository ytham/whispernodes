#include "Node.h"
#include <boost/algorithm/string.hpp>

#define MAX_BACKLOG     10
#define MAX_BUFFER_LEN  256

#define GET_PREFIX      "GET"
#define CONNECT_PREFIX  "CON"
#define MESSAGE_PREFIX  "MSG"
#define PREFIX_KEY      ' '

#define ORIGIN_NAME   "originName"
#define ORIGIN_PORT   "originPort"
#define FROM_NAME     "fromName"
#define FROM_PORT     "fromPort"
#define TARGET_NAME   "name"
#define MESSAGE_DATA  "message"

/*

Scenarios:

On Connect:
  - node will take list of bootnodes and connect to each one with CON prefix
  - CON prefix message sends name and port:
      CON$name=foo&port=3001
  - remote node will see it and add the node to the nodeMap_;
  - remote node will send back CON prefix with name and port
  - node will save that name and port to nodeMap_;

On Message Origination
  - node will parse HTTP GET
  - node will check if name on outgoing message is equal to its own name and if so will print the message
  - if not, the node will send the message to all nodes in nodeMap_;
      MSG$origin=foo@3001&name=baz&message=amessage

On Message Send:
  - node will check if name on the message is node's name and if so, display the message
  - if not, node will send message to all nodes in nodeMap_;

*/

Node::Node(std::string name, std::string port) : name_(name), port_(port) { }

std::string Node::GetName() {
  return name_;
}

std::string Node::GetPort() {
  return port_;
}

void Node::SetName(std::string name) {
  name_ = name;
}

int Node::GetSockfd() {
  return sockfd_;
}

void Node::SetSockfd(int sockfd) {
  sockfd_ = sockfd;
}

void Node::SetupListen() {
  int status, rv;
  int yes = 1;
  struct addrinfo hints, *serverinfo, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  std::string localhost = "127.0.0.1";

  if ((rv = getaddrinfo(localhost.c_str(), port_.c_str(), &hints, &serverinfo)) != 0) {
    std::cerr << "getaddrinfo error: " << rv << std::endl;
    return;
  }

  for (p = serverinfo; p != NULL; p = p->ai_next) {
    if ((sockfdListen_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      std::cerr << "Server: socket error" << std::endl;
    }
    if (setsockopt(sockfdListen_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      std::cerr << "Server: setsockopt error" << std::endl;
      exit(1);
    }
    if (bind(sockfdListen_, p->ai_addr, p->ai_addrlen) == -1) {
      std::cerr << "Server: bind error" << std::endl;
      close(sockfdListen_);
      continue;
    }
    break;
  }

  freeaddrinfo(serverinfo);

  if (listen(sockfdListen_, MAX_BACKLOG) != 0) {
    std::cerr << "Error listening on socket" << std::endl;
    exit(1);
  }
}

void Node::CheckIncoming() {
  char buf[MAX_BUFFER_LEN];
  struct sockaddr_storage remote_addr;

  // Accept any incoming connections
  socklen_t addr_size = sizeof(remote_addr);
  int new_fd = accept(sockfdListen_, (struct sockaddr*)&remote_addr, &addr_size);
  size_t bytesRead = recv(new_fd, buf, MAX_BUFFER_LEN, 0);
  // std::cout << "Bytes read: " << bytesRead << std::endl;
  // std::cout << "Buffer: \n" << buf << std::endl;

  Message message;
  memset(&message, 0, sizeof(Message));

  // Parse the incoming data into a Message struct
  ParseParams(&message, std::string(buf));

  // Handle the parsed Message struct
  HandleMessage(&message);
}


/** SECTION: PARSING **/

/**
 *  Parses parameters into a Message struct
 */
void Node::ParseParams(Message *message, std::string s) {
  std::vector<std::string> splitVector;
  boost::split(splitVector, s, boost::is_any_of(" "));
  DataType type = GetDataType(splitVector[0]);
  switch(type) {
    case Origination:
      message->type = Origination;
      ParseOriginationParams(message, splitVector[1]);
      break;
    case Connection:
      message->type = Connection;
    case Communication:
      message->type = Communication;
      ParseNodeToNodeParams(message, splitVector[1]);
      break;
    default:
      std::cerr << "Invalid Message type " << splitVector[1] << std::endl;
      break;
  }
}

// GET /whisper?name=tar&message=hallo HTTP/1.1
void Node::ParseOriginationParams(Message *message, std::string s) {
  std::vector<std::string> splitVector;
  boost::split(splitVector, s, boost::is_any_of("?"));
  std::vector<std::string> kvVector;
  boost::split(kvVector, splitVector[1], boost::is_any_of("&"));
  for (auto &item : kvVector) {
    ConstructMessage(message, item);
  }

  // Set current node's params as origin and from params
  strncpy(message->originName, name_.c_str(), name_.length());
  strncpy(message->originPort, port_.c_str(), port_.length());
  strncpy(message->fromName, name_.c_str(), name_.length());
  strncpy(message->fromPort, port_.c_str(), port_.length());
}

// CON name=foo&port=3001&
// MSG originName=foo&originPort=3001&name=baz&message=amessage&
void Node::ParseNodeToNodeParams(Message *message, std::string s) {
  std::vector<std::string> splitVector;
  boost::split(splitVector, s, boost::is_any_of("&"));
  for (auto &item : splitVector) {
    ConstructMessage(message, item);
  }

  // Set from field to current node's info
  strncpy(message->fromName, name_.c_str(), name_.length());
  strncpy(message->fromPort, port_.c_str(), port_.length());
}

void Node::ConstructMessage(Message *message, std::string s) {
  std::vector<std::string> splitVector;
  boost::split(splitVector, s, boost::is_any_of("="));
  if (splitVector[0] == ORIGIN_NAME) {
    strncpy(message->originName, splitVector[1].c_str(), splitVector[1].length());
  } else if (splitVector[0] == ORIGIN_PORT) {
    strncpy(message->originPort, splitVector[1].c_str(), splitVector[1].length());
  } else if (splitVector[0] == FROM_NAME) {
    strncpy(message->fromName, splitVector[1].c_str(), splitVector[1].length());
  } else if (splitVector[0] == FROM_PORT) {
    strncpy(message->fromPort, splitVector[1].c_str(), splitVector[1].length());
  }else if (splitVector[0] == TARGET_NAME) {
    strncpy(message->targetName, splitVector[1].c_str(), splitVector[1].length());
  } else if (splitVector[0] == MESSAGE_DATA) {
    strncpy(message->message, splitVector[1].c_str(), splitVector[1].length());
  } else {
    std::cerr << "Error: Invalid message item: " << splitVector[0] << std::endl;
  }
}


/** SECTION: MESSAGE HANDLING **/

void Node::HandleMessage(Message *message) {
  if (message->type == Origination) {
    // If it's an Origination message, print the initial message
    std::cout << "[" << name_ << "] Message sent to " << message->targetName << " at localhost:"
              << port_ << " :: " << message->message << std::endl;

    // Add the message to the queue to be sent to all connected nodes
    message->type = Communication;
    messageQueue_.push(*message);
    return;
  }

  std::string strTargetName(message->targetName);
  if (strTargetName == name_) {
    // Target found: print message
    std::cout << "[" << name_ << "] Message received from " << message->originName << " at localhost:"
              << message->originPort << " :: " << message->message << std::endl;
    return;
  }

  switch(message->type) {
    case Connection:
      // If it's a connection request, we should build the neighbor map
      AddToNeighborMap(message);
      break;
    case Communication:
      // If it's a communication, we should forward this message to all neighbors
      SendMessageToNeighbors(message);
      break;
    default:
      std::cerr << "Error: Invalid message type: " << message->type << std::endl;
      exit(1);
      break;
  }
}

void Node::AddToNeighborMap(Message *message) {
  std::string searchKey(message->fromPort);
  NodeMap::iterator it = neighborMap_.find(searchKey);
  if (it != neighborMap_.end()) {
    Node n(message->fromName, message->fromPort);
    neighborMap_.insert(std::pair<std::string, Node>(searchKey, n));
    Connect(&n);
    messageQueue_.push(*message);
  }
}

void Node::SendMessageToNeighbors(Message *message) {
  messageQueue_.push(*message);
}

/**
 *  Receives a comma-separated string of nodes (currently no error checking)
 */
void Node::AddNodes(std::string nodeName) {
  std::vector<std::string> results;
  boost::split(results, nodeName, [](char c){return c == ',';});
  for (auto &n : results) {
    std::vector<std::string> fullAddr;
    boost::split(fullAddr, n, [](char d){return d == ':';});
    std::string name = fullAddr[0];
    std::string port = fullAddr[1];
    NodeMap::iterator it = neighborMap_.find(port);
    if (it == neighborMap_.end()) {
      Node newNode(name, port);
      neighborMap_.insert(std::pair<std::string, Node>(port, newNode));
      // std::cout << "Added node with name " << name << " on port " << port << std::endl;
      Connect(&newNode);

      // Enqueue a Connection message
      Message message;
      memset(&message, 0, sizeof(Message));
      message.type = Connection;
      strncpy(message.fromName, name_.c_str(), MAX_MESSAGE_LENGTH);
      strncpy(message.fromPort, port_.c_str(), MAX_MESSAGE_LENGTH);
      SendMessage(newNode, &message);
    }
  }
}

Node::DataType Node::GetDataType(std::string prefix) {
  if (prefix == GET_PREFIX) {
    return Origination;
  }else if (prefix == CONNECT_PREFIX) {
    return Connection;
  } else if (prefix == MESSAGE_PREFIX) {
    return Communication;
  } else {
    return Invalid;
  }
}

/**
 *  Sends any pending messages in the queue
 */
void Node::SendMessageQueue() {
  while (!messageQueue_.empty()) {
    Message message = messageQueue_.front();
    messageQueue_.pop();
    for (auto it : neighborMap_) {
      Connect(&(it.second));
      SendMessage(it.second, &message);
    }
  }
}

void Node::SendMessage(Node n, Message *message) {
  std::string messageString = StringifyMessage(message);
  size_t bytesSent = send(n.GetSockfd(), messageString.c_str(), strlen(messageString.c_str()), 0);
  // std::cout << "messageString.c_str() size: " << strlen(messageString.c_str()) << std::endl;
  // std::cout << "Sent messageString: \"" << messageString << "\" of size " << bytesSent << " to node " << n.GetName() << ":" << n.GetPort() << std::endl;
}


std::string Node::DataTypeToPrefix(DataType type) {
  switch (type) {
    case Connection: return CONNECT_PREFIX;
    case Communication: return MESSAGE_PREFIX;
    default:
      std::cerr << "Invalid data type (" << type << "): no prefix available" << std::endl;
      exit(1);
  }
}

std::string Node::StringifyMessage(Message *message) {
  std::string originName = std::string(message->originName);
  std::string originPort = std::string(message->originPort);
  std::string fromName = std::string(message->fromName);
  std::string fromPort = std::string(message->fromPort);
  std::string targetName = std::string(message->targetName);
  std::string messageData = std::string(message->message);

  bool firstToken = true;
  std::stringstream ss;
  ss << DataTypeToPrefix(message->type) << PREFIX_KEY;
  if (originName.length() != 0) {
    if (!firstToken) {
      ss << "&";
    }
    ss << ORIGIN_NAME << "=" << originName;
    firstToken = false;
  }
  if (originPort.length() != 0) {
    if (!firstToken) {
      ss << "&";
    }
    ss << ORIGIN_PORT << "=" << originPort;
    firstToken = false;
  }
  if (fromName.length() != 0) {
    if (!firstToken) {
      ss << "&";
    }
    ss << FROM_NAME << "=" << fromName;
    firstToken = false;
  }
  if (fromPort.length() != 0) {
    if (!firstToken) {
      ss << "&";
    }
    ss << FROM_PORT << "=" << fromPort;
    firstToken = false;
  }
  if (targetName.length() != 0) {
    if (!firstToken) {
      ss << "&";
    }
    ss << TARGET_NAME << "=" << targetName;
    firstToken = false;
  }
  if (messageData.length() != 0) {
    if (!firstToken) {
      ss << "&";
    }
    ss << MESSAGE_DATA << "=" << messageData;
    firstToken = false;
  }
  return ss.str();
}

void Node::Connect(Node *n) {
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  std::string localhost = "127.0.0.1";
  getaddrinfo(localhost.c_str(), n->GetPort().c_str(), &hints, &res);

  int outsockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  n->SetSockfd(outsockfd);
  if (connect(outsockfd, res->ai_addr, res->ai_addrlen) != 0) {
    close(outsockfd);
    std::cerr << "Unable to connect to node at port " << n->GetPort() << std::endl;
    return;
  }
}

void Node::Shutdown() {
  close(sockfd_);
  close(sockfdListen_);
}
