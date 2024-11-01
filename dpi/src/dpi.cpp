#include "dpi.h"
#include <boost/thread.hpp>

std::mutex cout_lock;

void custom_set_highest_priority(std::thread& th, unsigned int subtract) {               
  sched_param sch;
  sch.sched_priority = sched_get_priority_max(SCHED_FIFO) - subtract;                      
  pthread_setschedparam(th.native_handle(), SCHED_FIFO, &sch);
}

DPI::DPI(const std::string& config_file) {
    init(config_file);
}

DPI::~DPI() {}

void DPI::init(const std::string& config_file) {
    std::ifstream dpi_config_file(config_file);
    dpi_config_file >> dpi_config;

    dpi_name = dpi_config["dpi_name"];

    std::ifstream ipfile("ip.txt");
    std::getline(ipfile, dpi_hostname);

    listen_port = dpi_config["dpi_bind_port"];


    allele_file_name = dpi_config["allele_file"];

    auto info = dpi_config["coordination_server_info"];
    send_msg(info["hostname"], info["port"], CoordinationServerMessageType::DPI_REGISTER, dpi_hostname + "\t" + std::to_string(listen_port));

    num_patients = 0;
    work_distributed_count = 0;
    y_and_cov_count = 0;
    filled_count = 0;
    sync_count = 0;
    verified_count = 0;
    cov_work_start = false;

    if (strcmp(ENCLAVE_PUBLIC_SIGNING_KEY, "Invalid") == 0) {
        throw std::runtime_error("The public signing key is not specified. See enclave_node/enclave/gen_pubkey_header.sh for an example of how to generate a proper header.");
    }
}

void DPI::run() {
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
    addr.sin_port = htons(listen_port);

    // bind to our given port, or randomly get one if port = 0
	if (bind(sockfd, (struct sockaddr*) &addr, addrSize) < 0) {
        guarded_cout("bind failure " + std::to_string(errno), cout_lock);
    } 

    // update our member variable to the port we just assigned
    if (getsockname(sockfd, (struct sockaddr*) &addr, &addrSize) < 0) {
        guarded_cout("getsockname failure: " + std::to_string(errno), cout_lock);
    }

    // (4) Begin listening for incoming connections.
	if (listen(sockfd, 4096) < 0) {
        guarded_cout("listen: " + std::to_string(errno), cout_lock);
    }

    listen_port = ntohs(addr.sin_port);
    guarded_cout("\n Running on port " + std::to_string(listen_port), cout_lock);

    // (5) Serve incoming connections one by one forever (a lonely fate).
	while (true) {
        // accept connection from dpi
        int connFD = accept(sockfd, (struct sockaddr*) &addr, &addrSize);

        // spin up a new thread to handle this message
        std::thread msg_thread(&DPI::start_thread, this, connFD);
        msg_thread.detach();
    }
}

bool DPI::start_thread(int connFD) {
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
                return false;
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
            return true;
        }
        unsigned int body_size;
        try {
            body_size = std::stoi(header);
        } catch (const std::invalid_argument &e) {
            std::cout << "Failed to read body size: " << header << std::endl;
            return true;
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

        // guarded_cout("ID: " + parsed_header[0] + 
        //              " Msg Type: " + parsed_header[1], cout_lock);
        // guarded_cout("\nEncrypted body:\n" + parsed_header[2], cout_lock);
        try {
            handle_message(connFD, std::stoi(parsed_header[0]), static_cast<DPIMessageType>(std::stoi(parsed_header[1])), parsed_header[2]);
        } catch (const std::invalid_argument &e) {
            std::cout << "Failed parse header type \n" << header << std::endl;
            return true;
        }
    }
    catch (const std::runtime_error& e)  {
        guarded_cout("Exception " + std::string(e.what()) + "\n", cout_lock);
        close(connFD);
        return false;
    }
    delete[] body_buffer;
    return true;
}

void DPI::handle_message(int connFD, const unsigned int global_id, const DPIMessageType mtype, std::string& msg) {

    std::string response;

    switch (mtype) {
        case DPI_INFO:
        {
            std::vector<std::string> parsed_dpi_info;
            Parser::split(parsed_dpi_info, msg, '\n');
            for (const std::string& dpi_info_str : parsed_dpi_info) {
                ConnectionInfo info;
                Parser::parse_connection_info(dpi_info_str, info, false);
                if (info.hostname != dpi_hostname || info.port != listen_port) {
                    dpi_info.push_back(info);
                }
            }
            // std::cout << dpi_info.size() << std::endl;
            break;
        }
        case ENCLAVE_INFO:
        {
            std::vector<std::string> parsed_enclave_info;
            Parser::split(parsed_enclave_info, msg, '\n');
            // All received data should be formatted as:
            // [hostname]   [port]   [thread count]
            int aes_idx = 0;
            const int num_enclave_nodes = parsed_enclave_info.size();
            aes_encryptor_list = std::vector<std::vector<AESCrypto> >(num_enclave_nodes);
            phenotypes_list.resize(num_enclave_nodes);
            allele_queue_list.resize(num_enclave_nodes);
            encryption_queue_list.resize(num_enclave_nodes);
            evidence_list.resize(num_enclave_nodes);
            // Mutexes are not movable apparently :/
            std::vector<std::mutex> tmp(num_enclave_nodes);
            encryption_queue_lock_list.swap(tmp);

            for (int idx = 0; idx < num_enclave_nodes; ++idx) {
                allele_queue_list[idx] = new std::queue<std::string>();
            } 

            for (const std::string& enclave_info : parsed_enclave_info) {
                ConnectionInfo info;
                Parser::parse_connection_info(enclave_info, info, true);
                enclave_node_info.push_back(info);

                // Create AES keys for each thread of this enclave node
                aes_encryptor_list[aes_idx++] = std::vector<AESCrypto>(info.num_threads);
            }
            for (unsigned int id = 0; id < enclave_node_info.size(); ++id) {
                evidence_list[id].buffer = nullptr;
                evidence_list[id].size = 0;
                send_msg(id, REGISTER, dpi_hostname + "\t" + std::to_string(listen_port));
            }

            break;
        }
        case EVIDENCE:
        {
            evidence_list[global_id].buffer = new uint8_t[msg.length()];
            std::memcpy(evidence_list[global_id].buffer, reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
            evidence_list[global_id].size = msg.length();
            break;
        }
        case RSA_PUB_KEY:
        {
	
            // Wait for the evidence if we didn't already recieve it
            while(!evidence_list[global_id].size) {
                std::this_thread::yield();
            }

            // Verify the evidence - we need to attest the enclave
            uint8_t pubkey_raw[RSA_PUB_KEY_SIZE];
            std::copy(msg.begin(), msg.end(), std::begin(pubkey_raw));
		std::cout<<"before verify_evidence\n";
		/*
            if (Attestation::verify_evidence(&evidence_list[global_id], pubkey_raw) != 0) {
                throw std::runtime_error("Failed to verify remote enclave!");
            }
		*/
            if (static_cast<unsigned long>(++verified_count) == evidence_list.size()) {
                std::cout << "All enclaves successfully attested and verified" << std::endl;
            }
		
            // I wanted to use .resize() but the compiler cried about it, this is not ideal but acceptable.
            
            const std::string header = "-----BEGIN PUBLIC KEY-----";
            const std::string footer = "-----END PUBLIC KEY-----";

            size_t pos1 = msg.find(header);
            size_t pos2 = msg.find(footer, pos1+1);
            if (pos1 == std::string::npos || pos2 == std::string::npos) {
                throw std::runtime_error("PEM header/footer not found");
            }
            // Start position and length
            pos1 = pos1 + header.length();
            pos2 = pos2 - pos1;
            CryptoPP::StringSource pub_key_source(aes_encryptor_list[global_id].front().decode(msg.substr(pos1, pos2)), true);
            CryptoPP::RSA::PublicKey public_key;
            public_key.Load(pub_key_source);
            
            CryptoPP::RSAES<CryptoPP::OAEP<CryptoPP::SHA256> >::Encryptor rsa_encryptor(public_key);
            for (unsigned int thread_id = 0; thread_id < enclave_node_info[global_id].num_threads; ++thread_id) {
                send_msg(global_id, AES_KEY, aes_encryptor_list[global_id][thread_id].get_key_and_iv(rsa_encryptor) + "\t" + std::to_string(thread_id));
            }
		
            break;
        }
        case Y_AND_COV:
        {
            std::vector<std::string> covariants;
            Parser::split(covariants, msg);
            
            // the last covariant is actually our Y value, remove it from the list
            std::string y_val = covariants.back();
            covariants.pop_back();

            prepare_tsv_file(global_id, y_val, Y_VAL);
            // send the data in each TSV file over to the server
            for (std::string covariant : covariants) {
                // Ignore requests for "1", this is handled within the enclave
                if (covariant != "1") {
                    prepare_tsv_file(global_id, covariant, COVARIANT);
                }
            }
            
            if (static_cast<unsigned int>(++y_and_cov_count) == aes_encryptor_list.size()) {
                fill_queue();
            }

            break;
        }
        case DATA_REQUEST:
        {   
            // Wait until all data is ready to go!
            std::mutex useless_lock;
            std::unique_lock<std::mutex> useless_lock_wrapper(useless_lock);
            while (static_cast<unsigned int>(filled_count) != allele_queue_list.size()) {
                start_sender_cv.wait(useless_lock_wrapper);
            }

            ConnectionInfo info = enclave_node_info[global_id];
            std::queue<std::string> *allele_queue = allele_queue_list[global_id];

            int blocks_sent = 0;
            std::string block;
            std::string lengths;

            std::string line;
            std::string line_length;
            int data_conn = -1;
            while (!allele_queue->empty()) {
                line = allele_queue->front();
                allele_queue->pop();
                line_length = "\t" + std::to_string(line.length());

                // 30 is magic number for extra padding
                int prospective_length = block.length() + line.length() + lengths.length() + line_length.length() + 30;
                if ((prospective_length > (1 << 16) - 1) && block.length()) {
                    // msg format: blocks sent \t lengths (tab delimited) \n (terminating char) blocks of data w no delimiters
                    std::string block_msg = std::to_string(blocks_sent++) + lengths + "\n" + block;
                    data_conn = send_msg(info.hostname, info.port, EnclaveNodeMessageType::DATA, block_msg, data_conn);

                    // Reset block
                    block.clear(); 
                    lengths.clear();
                }
                block += line;
                lengths += line_length;
            }
            // TODO: Re-evaluate why I compare it to 10? at some point this made a lot of sense to me, not it makes none
            // Send the leftover lines, 10 is an arbitary cut off. I assume most lines will be at least a few hundred characters
            // and we won't be sending more than 10^10 blocks
            if (block.length() > 10) {
                // msg format: blocks sent \t lengths (tab delimited) \n (terminating char) blocks of data w no delimiters
                std::string block_msg = std::to_string(blocks_sent++) + lengths + "\n" + block;
                send_msg(info.hostname, info.port, DATA, block_msg);
            }
            // If get_block failed we have reached the end of the file, send an EOF.
            send_msg(global_id, EOF_DATA, std::to_string(blocks_sent));


            if (global_id == 0) {
                std::cout << "Sending last message: "  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;

                auto stop = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
                std::cout << "Data send time total: " << duration.count() << std::endl;
            }
            break;
        }
        case DPI_SYNC: 
        {
            if (static_cast<unsigned int>(++sync_count) == dpi_info.size()) {
                sync_cv.notify_all();
            }
            break;
        }
        case END_DPI:
        {
            // DPI has served its purpose! Exit the program
            exit(0);
            break;
        }
        default:
            throw std::runtime_error("Not a valid response type");
    }
    if (mtype != DATA_REQUEST) {
        close(connFD);
    }
}

void DPI::send_msg(const unsigned int global_id, const unsigned int mtype, const std::string& msg, int connFD) {
    std::string message = dpi_name + " " + std::to_string(mtype) + " ";
    message = std::to_string(message.length() + msg.length()) + "\n" + message + msg;
    ConnectionInfo info = enclave_node_info[global_id];
    send_message(info.hostname.c_str(), info.port, message.data(), message.length(), connFD);
}

int DPI::send_msg(const std::string& hostname, unsigned int port, unsigned int mtype, const std::string& msg, int connFD) {
    std::string message = dpi_name + " " + std::to_string(mtype) + " ";
    message = std::to_string(message.length() + msg.length()) + "\n" + message + msg;
    return send_message(hostname.c_str(), port, message.data(), message.length(), connFD);
}

void DPI::queue_helper(const int global_id, const int num_helpers) {
    std::ifstream allele_file(allele_file_name);
    std::string line;
    // remove first line from file
    getline(allele_file, line);

    unsigned int line_num = 0;
    start = std::chrono::high_resolution_clock::now();

    // Put the helper file handler on an offset based on the id
    for (int i = 0; i < global_id; ++i) {
        allele_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        line_num++;
    }

    while (true) {
        if (getline(allele_file, line)) {
            // It's ok to have a benign data race here - each thread will do redundant writes of the same value.
            // I used to eliminate this redundancy by only having thread with id 0 do this write, but it is
            // theoretically (and practially...) possible for thread 0 to not get any lines causing
            // num_patients to never be written. This seems like the best solution!
            if (!num_patients) {
                // Subtract 2 for locus->alleles tab and alleles->first value tab
                std::vector<std::string> patients_split;
                Parser::split(patients_split, line, '\t');
                num_patients = patients_split.size() - 2;
            }

            EncryptionBlock *block = new EncryptionBlock();
            block->line_num = line_num++;
            block->line = line;
            unsigned int enclave_node_hash = Parser::parse_hash(block->line, aes_encryptor_list.size());
            encryption_queue_lock_list[enclave_node_hash].lock(); 
            encryption_queue_list[enclave_node_hash].push(block);
            encryption_queue_lock_list[enclave_node_hash].unlock();
        } else {
            allele_file.close();
            work_distributed_count++;
            queue_cv.notify_all();
            break;
        }

        // Skip forward a number of lines equal to the number of helper threads
        for (int i = 0; i < num_helpers - 1; ++i) {
            line_num++;
            // Skip ahead num patients (plus the tab delimeters), but this doesnt account for the
            // length of the locus + allele, so skip to the newline char after
            allele_file.seekg(num_patients * 2, std::ios_base::cur);
            allele_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    std::mutex useless_lock;
    std::unique_lock<std::mutex> useless_lock_wrapper(useless_lock);
    while (work_distributed_count != num_helpers) {
        queue_cv.wait(useless_lock_wrapper);
    }
    // We no longer need this thread if it is not in the encryption list range
    if (static_cast<unsigned int>(global_id) >= encryption_queue_list.size()) {
        return;
    }

    std::vector<uint8_t> vals;
    std::vector<uint8_t> compressed_vals;
    vals.resize(num_patients);
    compressed_vals.resize((num_patients / TWO_BIT_INT_ARR_SIZE) + (num_patients % TWO_BIT_INT_ARR_SIZE ? 1 : 0));
    while (encryption_queue_list[global_id].size()) {
        EncryptionBlock *block = encryption_queue_list[global_id].top();
        encryption_queue_list[global_id].pop();
        line = block->line;
        delete block;

        Parser::parse_allele_line(line, 
                                  vals, 
                                  compressed_vals, 
                                  aes_encryptor_list, 
                                  global_id);
        allele_queue_list[global_id]->push(line);
    }
    if (static_cast<unsigned int>(++filled_count) == allele_queue_list.size()) {
        // Make sure we read in at least one value
        int any = 0;
        for (unsigned int id = 0; id < enclave_node_info.size(); ++id){
            any ^= allele_queue_list[id]->size();
        }
        if (!any) {
            throw std::runtime_error("Empty allele file provided");
        }
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
        // Using guarded_cout is hard here because converting duration.count() to a string sucks
        cout_lock.lock();
        std::cout << "Fill/encryption time total: " << duration.count() << std::endl;
        cout_lock.unlock();

        // Spin up cov sender threads
        int id = 0;
        for (const std::vector<Phenotype>& phenotypes : phenotypes_list) {
            int priority_diff = 0;
            for (const Phenotype& ptype : phenotypes) {
                std::thread th([id, ptype, this]() {
                    std::mutex useless_lock;
                    std::unique_lock<std::mutex> useless_lock_wrapper(useless_lock);
                    while (!cov_work_start) {
                        start_sender_cv.wait(useless_lock_wrapper);
                    }
                    send_msg(id, ptype.mtype, ptype.message);
                });
                custom_set_highest_priority(th, priority_diff++);
                th.detach();
            }
            id++;
        }

        //  std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 10000));
        // Ok so this machine is finished, but we need to sync with the other dpis to help with timing accuracy
        for (ConnectionInfo info : dpi_info) {
            std::string msg = "";
            std::string message = "-2 " + std::to_string(DPIMessageType::DPI_SYNC) + " ";
            message = std::to_string(message.length() + msg.length()) + "\n" + message + msg;
            send_message(info.hostname.c_str(), info.port, message.data(), message.length());
        }
        
        while (static_cast<unsigned int>(sync_count) < dpi_info.size()) {
            std::mutex useless_lock;
            std::unique_lock<std::mutex> useless_lock_wrapper(useless_lock);
            sync_cv.wait(useless_lock_wrapper);
        }

        // Start timing of first message and wake up all threads!
        start = std::chrono::high_resolution_clock::now();
        // Casting duration.count() to a string sucks, so RAII is difficult here
        cout_lock.lock();
        std::cout << "Sending first message: "  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;
        cout_lock.unlock();

        cov_work_start = true;
        start_sender_cv.notify_all();
    }
}

void DPI::fill_queue() {
    const int num_helpers = std::max((unsigned int)boost::thread::hardware_concurrency(), (unsigned int)encryption_queue_list.size());
    for (int id = 0; id < num_helpers; ++id) {
        std::thread helper_thread(&DPI::queue_helper, this, id, num_helpers);
        helper_thread.detach();
    }
}

bool replace_str(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

void DPI::prepare_tsv_file(unsigned int global_id, const std::string& filename, EnclaveNodeMessageType mtype) {
    std::ifstream tsv_file("dpi_data/" + filename + ".tsv");
    std::string data;
    std::string line;

    std::vector<std::string> patient_and_data;
    int patient_count = -1;
    while(getline(tsv_file, line)) {
        patient_and_data.clear();
        Parser::split(patient_and_data, line, '\t');
        std::string val = patient_and_data.back();
        replace_str(val, "false", "0");
        replace_str(val, "true", "1");
        
        data.append(val + "\t");
        patient_count++;
    }
    data.pop_back();

    if (mtype == Y_VAL) {
        std::string patient_count_str = std::to_string(patient_count);
        patient_count_str = aes_encryptor_list[global_id].front().encrypt_line((byte *)&patient_count_str[0], patient_count_str.length());
        send_msg(global_id, EnclaveNodeMessageType::PATIENT_COUNT, patient_count_str);
    }

    // Some things are read by all threads (y values, covariants, etc.) and therefore 
    // should use the same AES key across all threads - we just thread id 0.
    data = aes_encryptor_list[global_id].front().encrypt_line((byte *)&data[0], data.length());
    if (mtype == COVARIANT) {
        data = filename + " " + data;
    }

    Phenotype ptype;
    ptype.message = data;
    ptype.mtype = mtype;
    phenotypes_list[global_id].push_back(ptype);
    tsv_file.close();
}
