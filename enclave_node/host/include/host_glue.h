#include "gwas.h"
/* OCALL */
int getdpinum();

bool getaes(const int dpi_num, const int thread_id, unsigned char key[256],
            unsigned char iv[256]);
int get_num_patients(const int dpi_num, char num_patients_buffer[ENCLAVE_SMALL_BUFFER_SIZE]);
int gety(const int dpi_num, char y[ENCLAVE_READ_BUFFER_SIZE]);
int getcov(const int dpi_num, const char cov_name[MAX_DPINAME_LENGTH],
           char cov[ENCLAVE_READ_BUFFER_SIZE]);
int getbatch(char batch[ENCLAVE_READ_BUFFER_SIZE], const int thread_id);