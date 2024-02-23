/*
 * Header file for our server class.
 */

#ifdef __APPLE__
#define MSG_NOSIGNAL 0x2000
#endif

#ifndef _coordination_server_H_
#define _coordination_server_H_

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <boost/thread.hpp>
#include <thread>
#include <condition_variable>
#include "json.hpp"
#include <iostream>
#include <future>
#include <fstream>
#include <atomic>
#include <assert.h>
#include <stdexcept>
#include <chrono>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "output.h"
#include "parser.h"
#include "concurrentqueue.h"
#include "socket_send.h"

#include <ctpl.h>

struct AlleleGT {
  inline bool operator()(const std::string &a, const std::string &b) const {
    // Months ago I wrote a sorter in python. I don't remember why it works (or if it's 100% correct?)
    // but here is the C++ implementation. Not optimized in the slightest and horrible to read - enjoy!
    std::vector<std::string> a_split;
    std::vector<std::string> b_split;
    std::vector<std::string> a_split2;
    std::vector<std::string> b_split2;

    Parser::split(a_split, a, ':');
    Parser::split(b_split, b, ':');
    int a_loci_1 = a_split[0] == "X" ? 24 : std::stoi(a_split[0]);
    int b_loci_1 = b_split[0] == "X" ? 24 :std::stoi(b_split[0]);
    Parser::split(a_split2, a_split[1], '\t');
    Parser::split(b_split2, b_split[1], '\t');
    std::string a_loci_2 = a_split2[0];
    std::string b_loci_2 = b_split2[0];

    if (a_loci_1 == b_loci_1) {
      if (a_loci_2.length() != b_loci_2.length()) {
        if (a_loci_2.length() < b_loci_2.length()) {
          return false;
        } else {
          return true;
        }
      }
      if (a_loci_2 > b_loci_2) {
        return true;
      }
      return false;
    }
    return a_loci_1 > b_loci_1;
  }
};

class CoordinationServer {
  private:
    unsigned int port;
    unsigned int enclave_node_count;
    unsigned int dpi_count;
    std::atomic<int> eof_messages_received;
    nlohmann::json coordination_config;

    std::vector<std::string> enclave_node_info;
    std::vector<std::string> dpi_info;
    std::mutex enclave_lock;

    std::vector<ConnectionInfo> institution_info_list;
    std::vector<ConnectionInfo> enclave_info_list;

    std::vector<moodycamel::ConcurrentQueue<std::string> > tmp_file_string_list;

    std::vector<std::mutex> tmp_file_mutex_list;

    std::vector<bool> got_msg;

    moodycamel::ConcurrentQueue<int> work_queue; 
    ctpl::thread_pool t_pool;
    bool shutdown;

    std::priority_queue<std::string, std::vector<std::string>, AlleleGT > sorted_file_queue;

    std::ofstream output_file;

    std::string output_file_name;

    bool first;
    std::atomic<bool> eof_rec;

    // set up data structures
    void init(const std::string& config_file);
    
    // parses and calls the appropriate handler for an incoming dpi request
    bool handle_message(int connFD, CoordinationServerMessageType mtype, std::string& msg, std::string global_id);

    // send messages to the dpi
    int send_msg(const std::string& hostname, const int port, int mtype, const std::string& msg, int connFD=-1);

    // start a thread that will handle a message and exit properly if it finds an error
    void start_thread();

    void debug_eof();

  public:

    CoordinationServer(const std::string& config_file);

    ~CoordinationServer();

    // create listening socket to handle requests on indefinitely
    void run();
};

#endif /* _coordination_server_H_ */
