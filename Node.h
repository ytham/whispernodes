#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <unordered_map>

#define MAX_NAME_LENGTH 32
#define MAX_PORT_LENGTH 6 // Max length of uint16_t + 1 byte for null terminating char
#define MAX_MESSAGE_LENGTH 64

class Node {
  public:
    using DataType = enum {
      Origination,
      Connection,
      Communication,
      Invalid
    };

    using Message = struct {
      DataType type;
      char originName[MAX_NAME_LENGTH];
      char originPort[MAX_PORT_LENGTH];
      char fromName[MAX_NAME_LENGTH];
      char fromPort[MAX_PORT_LENGTH];
      char targetName[MAX_NAME_LENGTH];
      char message[MAX_MESSAGE_LENGTH];
    };

    Node(std::string name, std::string port);
    std::string GetName();
    std::string GetPort();
    void SetName(std::string);
    int GetSockfd();
    void SetSockfd(int sockfd);

    void SetupListen();
    void CheckIncoming();
    void AddNodes(std::string nodeString);
    void SendMessageQueue();
    void Shutdown();

  private:
    const std::string PREFIX_KEY = " ";

    using NodeMap = std::unordered_map<std::string, Node>;
    NodeMap neighborMap_;
    
    std::string name_;
    std::string port_;
    int sockfdListen_;
    int sockfd_;

    std::queue<Message> messageQueue_;

    std::string GetParams(std::string s);
    DataType GetDataType(std::string data);
    void ParseParams(Message *message, std::string s);
    void ParseOriginationParams(Message *message, std::string s);
    void ParseNodeToNodeParams(Message *message, std::string s);
    void ConstructMessage(Message *message, std::string s);
    void HandleMessage(Message *message);
    void AddToNeighborMap(Message *message);
    void SendMessageToNeighbors(Message *message);

    void Connect(Node *n);
    void SendMessage(Node n, Message *message);
    std::string DataTypeToPrefix(DataType type);
    std::string StringifyMessage(Message *message);
};
