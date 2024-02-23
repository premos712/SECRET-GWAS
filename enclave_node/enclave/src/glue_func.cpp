#include "enclave_glue.h"
#include "../../host/include/host_glue.h"

void getdpinum(int* _retval) { *_retval = getdpinum(); }

void getaes(bool* _retval, const int dpi_num, const int thread_id,
            unsigned char key[256], unsigned char iv[256]){
    *_retval = getaes(dpi_num, thread_id, key, iv);
}

void get_num_patients(int* _retval, const int dpi_num, 
                      char num_patients_buffer[ENCLAVE_SMALL_BUFFER_SIZE]) {
    *_retval = get_num_patients(dpi_num, num_patients_buffer);
}

void gety(int* _retval, const int dpi_num, 
    char y[ENCLAVE_READ_BUFFER_SIZE]){
    *_retval = gety(dpi_num, y);
}

void getcov(int* _retval, const int dpi_num,
            const char cov_name[MAX_DPINAME_LENGTH],
            char cov[ENCLAVE_READ_BUFFER_SIZE]){
    *_retval = getcov(dpi_num, cov_name, cov);
}

void getbatch(int* _retval, char batch[ENCLAVE_READ_BUFFER_SIZE],
              const int thread_id){
    *_retval = getbatch(batch, thread_id);
}

void mark_eof_wrapper(const int thread_id) {

}