#include <boost/program_options.hpp>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include "Node.h"
#include "ErrorTypes.h"

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
  try {
    po::options_description desc("Program Options");
    desc.add_options()
      ("name", po::value<std::string>(), "Client name")
      ("port", po::value<std::string>(), "port to use")
      ("bootnodes", po::value<std::string>(), "Nodes to connect to")
    ;

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);
    }
    catch(po::error &e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return ERR_PROGRAM_OPTIONS;
    }

    // Required options are name and port
    if(!vm.count("name") || !vm.count("port")) return ERR_REQUIRED_OPTION;

    std::string name = vm["name"].as<std::string>();
    std::string port = vm["port"].as<std::string>();
    Node n(name, port);
    std::cout << "Created node " << name << " on port " << port << std::endl;

    if(vm.count("bootnodes")) {
      // If bootnodes are included, add them to the new node
      n.AddNodes(vm["bootnodes"].as<std::string>());
    }

    n.SetupListen();

    while(true) {
      // Main run loop
      n.CheckIncoming();
      n.SendMessageQueue();
    }
  }
  catch(std::exception &e) {
    std::cerr << "Unhandled exception: " << e.what() << std::endl;
    return ERR_UNHANDLED;
  }

  return 0;
}
