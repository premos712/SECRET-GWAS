#include "batch.h"
#include "buffer.h"

#include "logistic_regression.h"
#include "string.h"
#include <map>
#include <thread>

void aes_decrypt_dpi(const unsigned char* crypto, unsigned char* plaintxt, const DPIInfo& dpi, const int thread_id) {
    aes_decrypt_data(dpi.aes_list[thread_id].aes_context,
                     (unsigned char *)dpi.aes_list[thread_id].aes_iv,
                     crypto,
                     dpi.crypto_size - 1, 
                     plaintxt);
}

void two_bit_decompress(uint8_t* input, uint8_t* decompressed, unsigned int size) {
    int input_idx = 0;
    int two_bit_arr_count = 0;
    for (int decompressed_idx = 0; decompressed_idx < size; ++decompressed_idx) {
        decompressed[decompressed_idx] = input[input_idx] & 0b11;
        if (++two_bit_arr_count == TWO_BIT_INT_ARR_SIZE) {
            input_idx++;
            two_bit_arr_count = 0;
        } else {
            input[input_idx] >>= 2;
        }
    }
}

void Buffer::decrypt_line(char* plaintxt, size_t* plaintxt_length, unsigned int num_lines, const std::vector<DPIInfo>& dpi_info_list, const int thread_id) {
    char* crypt_head = crypttxt; 
    char *crypt_start, *end_of_allele, *end_of_loci;
    char* plaintxt_head = plaintxt;
    for (int line = 0; line < num_lines; ++line) {
        crypt_start = crypt_head;
        end_of_allele = crypt_head;
        end_of_loci = crypt_head;
        while (true) {
            if (*crypt_head == '\t') {
                if (end_of_loci != crypt_start) {
                    end_of_allele = crypt_head;
                    break;
                } else {
                    end_of_loci = crypt_head;
                }
            }
            crypt_head++;
        }
        /* copy allele & loci to plaintxt */
        strncpy(plaintxt_head, crypt_start, end_of_allele - crypt_start + 1);
        plaintxt_head += end_of_allele - crypt_start + 1;

        char* tab_pos = end_of_allele;
        dpi_count = 0;
        /* get dpi list */
        crypt_head++;
        while (true) {
            if(*crypt_head == '\t'){
                *crypt_head = '\0';
                int dpi = atoi(tab_pos + 1);
                dpi_list[dpi_count++] = dpi;
                *crypt_head = '\t';
                tab_pos = crypt_head;
            }
            if (*crypt_head == ' ') {
                *crypt_head = '\0';
                int dpi = atoi(tab_pos + 1);
                dpi_list[dpi_count++] = dpi;
                *crypt_head = ' ';
                crypt_head++;
                break;
            }
            crypt_head++;
        }
        /* decrypt data */
        for (int i = 0; i < dpi_count; i++){
            dpi_crypto_map[dpi_list[i]] = crypt_head;
            crypt_head += dpi_info_list[dpi_list[i]].crypto_size;
        }
        bool dpi_found;
        for (int dpi = 0; dpi < dpi_info_list.size(); dpi++) {
            dpi_found = false;
            for (int list_id = 0; list_id < dpi_count; ++list_id) {
                if (dpi_list[list_id] == dpi) {
                    aes_decrypt_dpi((const unsigned char*)dpi_crypto_map[list_id],
                                       (unsigned char*)plaintxt_head,
                                       dpi_info_list[dpi], 
                                       thread_id);
                    // two_bit_decompress(plain_txt_compressed, 
                    //                    (uint8_t*)plaintxt_head, 
                    //                    dpi_info_list[dpi].size);
                    // memset(plain_txt_compressed, 0, ENCLAVE_READ_BUFFER_SIZE);
                    // memset(dpi_crypto_map[list_id], 0, dpi_info_list[dpi].crypto_size);
                    dpi_found = true;
                }
            }
            if (!dpi_found) {
                // this dpi does have target allele
                for (int j = 0; j < dpi_info_list[dpi].size; j++) {
                    *(plaintxt_head + j) = NA_byte; // set whole byte to 0b11111111
                }
            }
            plaintxt_head += dpi_info_list[dpi].size;
        }
        *plaintxt_head = '\n';
        plaintxt_head++;
    }
    *plaintxt_head = '\0';
    *plaintxt_length = plaintxt_head - plaintxt;
}

Buffer::Buffer(size_t _row_size, EncAnalysis type, int num_dpis, int _thread_id)
    : row_size(_row_size), analysis_type(type), thread_id(_thread_id) {
    crypttxt = new char[ENCLAVE_READ_BUFFER_SIZE];
    plain_txt_compressed = new uint8_t[ENCLAVE_READ_BUFFER_SIZE];
    dpi_list = new int[num_dpis];
    dpi_crypto_map = new char* [num_dpis];
    // I now remember why we do this! Because we do batching, we can load in ENCLAVE_READ_BUFFER_SIZE
    // amount of data in at a time, BUT this data when decompressed can actually be up to 4 * ENCLAVE_READ_BUFFER_SIZE large
    plaintxt_buffer = new char[ENCLAVE_READ_BUFFER_SIZE];
    output_tail = 0;
    eof = false;

    memset(crypttxt, 0, ENCLAVE_READ_BUFFER_SIZE);
    memset(plain_txt_compressed, 0, ENCLAVE_READ_BUFFER_SIZE);
    memset(plaintxt_buffer, 0, ENCLAVE_READ_BUFFER_SIZE);
}

Buffer::~Buffer() {
    delete free_batch;
    delete [] dpi_list;
    delete [] dpi_crypto_map;
}

void Buffer::mark_eof() {
    eof = true;
}

void Buffer::add_gwas(GWAS* _gwas, ImputePolicy impute_policy, const std::vector<int>& sizes) {
    free_batch = new Batch(row_size, analysis_type, impute_policy, _gwas, plaintxt_buffer, sizes, thread_id);
}

void Buffer::output(const char* out, const size_t& length) {
    if (output_tail + length >= ENCLAVE_READ_BUFFER_SIZE) {
        writebatch(output_buffer, output_tail, thread_id);
        memset(output_buffer, 0, ENCLAVE_READ_BUFFER_SIZE);
        output_tail = 0;
    }
    strcpy(output_buffer + output_tail, out);
    output_tail += length;
}

void Buffer::clean_up() {
    if (output_tail > 0) {
        writebatch(output_buffer, output_tail, thread_id);
    }
}

void Buffer::finish() {
    output(free_batch->output_buffer(), free_batch->get_out_tail());
    free_batch->reset();
}

Batch* Buffer::launch(std::vector<DPIInfo>& dpi_info_list, const int thread_id) {
    int num_lines = 0;
    while (!num_lines) {
        getbatch(&num_lines, crypttxt, thread_id);
        if (eof) {
            return nullptr;
        }
        if (!num_lines) {
            std::this_thread::yield();
        }
    }
    if (num_lines == -1) {
        return nullptr;
    }
    //if (!strcmp(crypttxt, EOFSeperator)) return nullptr;
    if (!free_batch) return nullptr;
    *free_batch->plaintxt_size() = 0;
    decrypt_line(free_batch->load_plaintxt(), free_batch->plaintxt_size(), num_lines, dpi_info_list, thread_id);
    return free_batch;
}