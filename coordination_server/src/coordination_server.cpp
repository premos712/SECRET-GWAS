/*
 * Implementation for our server class.
 */

#include "coordination_server.h"
#include "ctpl.h"

std::mutex cout_lock;
std::condition_variable work_queue_condition;

CoordinationServer::CoordinationServer(const std::string& config_file) {
    init(config_file);
}

CoordinationServer::~CoordinationServer() {

}

void CoordinationServer::init(const std::string& config_file) {
    std::ifstream coordination_config_file(config_file);
    coordination_config_file >> coordination_config;
    port = coordination_config["coordination_server_bind_port"];
    enclave_node_count = coordination_config["enclave_node_count"];
    dpi_count = coordination_config["dpi_count"];
    output_file_name = coordination_config["output_file_name"];
    output_file.open(output_file_name);
    
    tmp_file_string_list.resize(enclave_node_count);

    std::vector<std::mutex> mux_tmp(enclave_node_count);
    tmp_file_mutex_list.swap(mux_tmp);

    eof_messages_received = 0;
    first = true;
    shutdown = false;
    t_pool.resize(std::thread::hardware_concurrency());
    got_msg.resize(enclave_node_count);
    std::fill(got_msg.begin(), got_msg.end(), false);

    eof_rec = false;
}

void CoordinationServer::run() {
    // set up the server to do nothing when it receives a broken pipe error
    //signal(SIGPIPE, signal_handler);
    // create a master socket to listen for requests on
    // (1) Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // (2) Set the "reuse port" socket option
	int yesval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval));

    // (3) Create a sockaddr_in struct for the proper port and bind() to it.
	struct sockaddr_in addr;
    socklen_t addrSize = sizeof(addr);
    memset(&addr, 0, addrSize);

    // specify socket family (Internet)
	addr.sin_family = AF_INET;

	// specify socket hostname
	// The socket will be a server, so it will only be listening.
	// Let the OS map it to the correct address.
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    // bind to our given port, or randomly get one if port = 0
	if (bind(sockfd, (struct sockaddr*) &addr, addrSize) < 0) {
        guarded_cout("bind failure: " + std::to_string(errno), cout_lock);
    } 

    // update our member variable to the port we just assigned
    if (getsockname(sockfd, (struct sockaddr*) &addr, &addrSize) < 0) {
        guarded_cout("getsockname failure: " + std::to_string(errno), cout_lock);
    }

    // (4) Begin listening for incoming connections.
	if (listen(sockfd, 4096) < 0) {
        guarded_cout("listen: " + std::to_string(errno), cout_lock);
    }

    port = ntohs(addr.sin_port);
    guarded_cout("\n Running on port " + std::to_string(port), cout_lock);


    // Make a thread pool to listen for connections!
    // const uint32_t num_threads = 2 * std::thread::hardware_concurrency(); // Max # of threads the system supports
    // for (uint32_t ii = 0; ii < num_threads; ++ii) {
    //     std::thread pool_thread = std::thread(&CoordinationServer::start_thread_wrapper, this);
    //     pool_thread.detach();
    // }
    

    // (5) Serve incoming connections one by one forever (a lonely fate).
	while (true) {
        // accept connection from dpi
        int connFD = accept(sockfd, (struct sockaddr*) &addr, &addrSize);
        
        work_queue.enqueue(connFD);
        t_pool.push(
            [&, this](int id){ this->start_thread(); }
        );

        // spin up a new thread to handle this message
        // boost::thread msg_thread(&CoordinationServer::start_thread, this, connFD);
        // msg_thread.detach();
    }
}

void CoordinationServer::start_thread() {
    int connFD;
    if (!work_queue.try_dequeue(connFD)) {
        throw std::runtime_error("should be impossible? didn't find connFD");
    }
    char* body_buffer = new char[MAX_MESSAGE_SIZE]();
    // if we catch any errors we will throw an error to catch and close the connection
    try {
        char header_buffer[128];
        // receive header, byte by byte until we hit deliminating char
        memset(header_buffer, 0, sizeof(header_buffer));

        int header_size = 0;
        bool found_delim = false;
        while (header_size < 128) {
            // Receive exactly one byte
            int rval = recv(connFD, header_buffer + header_size, 1, MSG_WAITALL);
            if (rval == -1) {
                throw std::runtime_error("Socket recv failed\n");
            } else if (rval == 0) {
                return;
            }
            // Stop if we received a deliminating character
            if (header_buffer[header_size] == '\n') {
                found_delim = true;
                break;
            }
            header_size++;
        }
        if (!found_delim) {
            throw std::runtime_error("Didn't read in a null terminating char");
        }
        std::string header(header_buffer, header_size);

        if (header.find("GET / HTTP/1.1") != std::string::npos) {
            std::cout << "Strange get request? Ignoring for now." << std::endl;
            delete[] body_buffer;
            return;
        }

        unsigned int body_size;
        try {
            body_size = std::stoi(header);
        } catch(const std::invalid_argument& e) {
            std::cout << "Failed to read in body size" << std::endl;
            std::cout << header << std::endl;
            return;
        }
        
        if (body_size != 0) {
            // read in encrypted body
            int rval = recv(connFD, body_buffer, body_size, MSG_WAITALL);
            if (rval == -1) {
                throw std::runtime_error("Error reading request body");
            }
        }
        std::string body(body_buffer, body_size);
        std::vector<std::string> parsed_header;
        Parser::split(parsed_header, body, ' ', 2);

        CoordinationServerMessageType type = static_cast<CoordinationServerMessageType>(std::stoi(parsed_header[1]));
        // if (type != CoordinationServerMessageType::EOF_OUTPUT && type != CoordinationServerMessageType::OUTPUT) {
        //     cout_lock.lock();
        //     std::cout << "ID/DPI: " << parsed_header[0] 
        //               << " Msg Type: " << parsed_header[1] << "\n";
        //     cout_lock.unlock();
        // }
        //guarded_cout("\nEncrypted body:\n" + parsed_header[2], cout_lock);
        handle_message(connFD, type, parsed_header[2], parsed_header[0]);
    }
    catch (const std::runtime_error& e)  {
        guarded_cout("Exception " + std::string(e.what()) + "\n", cout_lock);
        close(connFD);
        return;
    }
    delete[] body_buffer;
    return;
}

void CoordinationServer::debug_eof() {
    std::this_thread::sleep_for(std::chrono::milliseconds(30000));
    for (unsigned int i = 0; i < got_msg.size(); ++i) {
        if (!got_msg[i]) {
            std::cout << "No rec value for " << i << std::endl;
         }
    }
}

bool CoordinationServer::handle_message(int connFD, CoordinationServerMessageType mtype, std::string& msg, std::string global_id) {
    std::string response;

    switch (mtype) {
        case ENCLAVE_REGISTER:
        {
            ConnectionInfo enclave_info;
            // Compare the max thread count of this machine with the others before it
            Parser::parse_connection_info(msg, enclave_info);
            // Send the enclave node its global ID
            enclave_lock.lock();
            int curr_enclave_node_info_size = enclave_node_info.size();

            enclave_info_list.push_back(enclave_info);
            enclave_node_info.push_back(msg);
            if (enclave_node_info.size() == enclave_node_count && dpi_info.size() == dpi_count) {
                std::string serialized_server_info;
                std::string serialized_dpi_info;

                // Create message containing all enclave node info
                for (std::string& info : enclave_node_info) {
                    serialized_server_info.append(info + "\n");
                }
                // Remove trailing '\t'
                serialized_server_info.pop_back();

                // Create message containing all dpi server info
                for (std::string& info : dpi_info) {
                    serialized_dpi_info.append(info + "\n");
                }

                // Remove trailing '\t'
                serialized_dpi_info.pop_back();

                // Send the enclave node info to all waiting dpis
                for (ConnectionInfo institution_info : institution_info_list) {
                    send_msg(institution_info.hostname, institution_info.port, DPIMessageType::DPI_INFO, serialized_dpi_info);
                    send_msg(institution_info.hostname, institution_info.port, DPIMessageType::ENCLAVE_INFO, serialized_server_info);
                }
            }
            std::cout << enclave_info.hostname << " " << std::to_string(curr_enclave_node_info_size) << std::endl;
            enclave_lock.unlock();

            send_msg(enclave_info.hostname, enclave_info.port, EnclaveNodeMessageType::GLOBAL_ID, std::to_string(curr_enclave_node_info_size));

            break;
        }
        case DPI_REGISTER:
        {   
            guarded_cout("Got dpi " + global_id, cout_lock);
            // Parse body to get hostname and port
            ConnectionInfo institution_info;
            Parser::parse_connection_info(msg, institution_info);

            std::lock_guard<std::mutex> raii(enclave_lock);
            // If we don't have all enclave node info, add to waiting queue
            institution_info_list.push_back(institution_info);
            dpi_info.push_back(msg);
            if (enclave_node_info.size() == enclave_node_count && dpi_info.size() == dpi_count) {
                std::string serialized_server_info;
                std::string serialized_dpi_info;

                // Create message containing all enclave node info
                for (std::string& info : enclave_node_info) {
                    serialized_server_info.append(info + "\n");
                }
                // Remove trailing '\t'
                serialized_server_info.pop_back();

                // Create message containing all dpi server info
                for (std::string& info : dpi_info) {
                    serialized_dpi_info.append(info + "\n");
                }

                // Remove trailing '\t'
                serialized_dpi_info.pop_back();

                // Send the enclave node info to all waiting dpis
                for (ConnectionInfo institution_info : institution_info_list) {
                    send_msg(institution_info.hostname, institution_info.port, DPIMessageType::DPI_INFO, serialized_dpi_info);
                    send_msg(institution_info.hostname, institution_info.port, DPIMessageType::ENCLAVE_INFO, serialized_server_info);
                }
            }
            break;
        }
        case OUTPUT:
        {
            int id = std::stoi(global_id);
            moodycamel::ConcurrentQueue<std::string> &tmp_file_string = tmp_file_string_list[id];
            tmp_file_string.enqueue(msg);
            break;
        }
        case EOF_OUTPUT:
        {
            int id = std::stoi(global_id);
            got_msg[id] = true;
            if (!eof_rec.exchange(true)) {
                boost::thread msg_thread(&CoordinationServer::debug_eof, this);
                msg_thread.detach();
            }
            if (strcmp(msg.c_str(), EOFSeperator) != 0) {
                moodycamel::ConcurrentQueue<std::string> &tmp_file_string = tmp_file_string_list[id];
                tmp_file_string.enqueue(msg);
            }
            
            if (static_cast<unsigned int>(++eof_messages_received) == enclave_node_count) {
                std::cout << "Received last message: "  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                for (moodycamel::ConcurrentQueue<std::string>& tmp_file_string : tmp_file_string_list) {
                    std::string tmp;
                    while (tmp_file_string.try_dequeue(tmp)) {
                        std::vector<std::string> split;
                        Parser::split(split, tmp, '\n');
                        for (const std::string& tmp_split : split) {
                            sorted_file_queue.push(tmp_split);
                        }
                    }
                }

                while(!sorted_file_queue.empty()) {
                    output_file << sorted_file_queue.top() << std::endl;
                    sorted_file_queue.pop();
                }

                output_file.flush();

                std::vector<std::thread> msg_threads;
                // These aren't necesary for program correctness, but they help with iterative testing!
                for (ConnectionInfo institution_info : institution_info_list) {
                    msg_threads.push_back(std::thread([institution_info, this]() {
                        send_msg(institution_info.hostname, institution_info.port, DPIMessageType::END_DPI, "");
                    }));
                }
                for (ConnectionInfo enclave_info : enclave_info_list) {
                    msg_threads.push_back(std::thread([enclave_info, this]() {
                        send_msg(enclave_info.hostname, enclave_info.port, EnclaveNodeMessageType::END_ENCLAVE, "");
                    }));
                }
                for (std::thread &t : msg_threads) {
                    t.join();
                }

                // All files recieved, all shutdown messages sent, we can exit now
                exit(0);
            }
            break;
        }
        default:
            throw std::runtime_error("Not a valid response type");
    }
    close(connFD);
    return false;
}

int CoordinationServer::send_msg(const std::string& hostname, const int port, int mtype, const std::string& msg, int connFD) {
    std::string message = "-1rs " + std::to_string(mtype) + " ";
    message = std::to_string(message.length() + msg.length()) + "\n" + message + msg;
    return send_message(hostname.c_str(), port, message.c_str(), message.length(), connFD);
}