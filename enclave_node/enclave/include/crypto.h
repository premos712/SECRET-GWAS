// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef CRYPTO_H
#define CRYPTO_H

// Includes for mbedtls shipped with oe.
// Also add the following libraries to your linker command line:
// -loeenclave -lmbedcrypto -lmbedtls -lmbedx509
#include <mbedtls/aes.h>
#include <mbedtls/config.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>

// #include "mbedtls_utility.h"
#include <openenclave/attestation/attester.h>
#include <openenclave/attestation/sgx/evidence.h>
#include <openenclave/attestation/sgx/report.h>
#include <openenclave/enclave.h>
#include <openenclave/attestation/verifier.h>
#include <openenclave/attestation/custom_claims.h>

#include "buffer_size.h"

#include <iostream>

inline void TRACE_ENCLAVE(const char* str) {
    std::cout << str << std::endl;
}

static const oe_uuid_t sgx_remote_uuid = {OE_FORMAT_UUID_SGX_ECDSA};

struct AESData {
    mbedtls_aes_context* aes_context;
    unsigned char aes_key[AES_KEY_LENGTH];
    unsigned char aes_iv[AES_IV_LENGTH];
};

struct buffer_t {
    uint8_t* buffer;
    size_t size;
};

void aes_decrypt_data(mbedtls_aes_context* aes_context, 
                      unsigned char* aes_iv, 
                      const unsigned char* input_data, 
                      int input_size, 
                      unsigned char* output_data);

class RSACrypto {
    private:
      mbedtls_ctr_drbg_context m_ctr_drbg_context;
      mbedtls_entropy_context m_entropy_context;
      mbedtls_pk_context m_pk_context;
      uint8_t m_public_key[RSA_PUB_KEY_SIZE];

    public:
        bool m_initialized;
        RSACrypto();
        ~RSACrypto();

        /**
         * decrypt decrypts the given data using current enclave's private key.
         * Used to receive encrypted data from another enclave.
         */
        bool decrypt(
            const uint8_t* encrypted_data,
            size_t encrypted_data_size,
            uint8_t* data,
            size_t* data_size);

        /**
         * Compute the sha256 hash of given data.
         */
        int sha256(const uint8_t* data, size_t data_size, uint8_t sha256[32]);

        int get_enclave_format_settings(const oe_uuid_t* format_id, buffer_t* format_settings);
        
        int get_evidence(buffer_t* evidence);

        bool generate_attestation_evidence(uint8_t** evidence, size_t* evidence_size);

        uint8_t* get_pub_key();

  private:
      /** init_mbedtls initializes the crypto module.
       */
      bool init_mbedtls(void);

      void cleanup_mbedtls(void);
};

#endif // OE_SAMPLES_ATTESTATION_ENC_CRYPTO_H