// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#include "crypto.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>

void aes_decrypt_data(mbedtls_aes_context* aes_context, 
                      unsigned char* aes_iv, 
                      const unsigned char* input_data, 
                      int input_size, 
                      unsigned char* output_data) {
    int ret = mbedtls_aes_crypt_cbc(
                aes_context,
                MBEDTLS_AES_DECRYPT,
                input_size, // input data length in bytes,
                aes_iv, // Initialization vector (updated after use)
                input_data,
                output_data);
    if (ret != 0) {
        std::cout << "Decryption failed with error: " << ret << std::endl;
        std::cout << "Input size " << input_size << std::endl;
        exit(0);
    }
}

RSACrypto::RSACrypto() {
    m_initialized = false;
    int res = -1;

    mbedtls_ctr_drbg_init(&m_ctr_drbg_context);
    mbedtls_entropy_init(&m_entropy_context);
    mbedtls_pk_init(&m_pk_context);

    // Initialize entropy.
    res = mbedtls_ctr_drbg_seed(
        &m_ctr_drbg_context,
        mbedtls_entropy_func,
        &m_entropy_context,
        nullptr,
        0);
    if (res != 0) {
        return;
    }

    // Initialize RSA context.
    res = mbedtls_pk_setup(&m_pk_context, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (res != 0) {
        return;
    }

    // Generate an ephemeral 2048-bit RSA key pair with
    // exponent 65537 for the enclave.
    res = mbedtls_rsa_gen_key(
        mbedtls_pk_rsa(m_pk_context),
        mbedtls_ctr_drbg_random,
        &m_ctr_drbg_context,
        2048,
        65537);
    if (res != 0) {
        return;
    }

    // Write out the public key in PEM format for exchange with other enclaves.
    res = mbedtls_pk_write_pubkey_pem(&m_pk_context, m_public_key, sizeof(m_public_key));
    if (res != 0) {
        return;
    }
    m_initialized = true;
}

RSACrypto::~RSACrypto() {
    mbedtls_pk_free(&m_pk_context);
    mbedtls_entropy_free(&m_entropy_context);
    mbedtls_ctr_drbg_free(&m_ctr_drbg_context);
}

// Compute the sha256 hash of given data.
int RSACrypto::sha256(const uint8_t* data, size_t data_size, uint8_t sha256[32]) {
    int ret = 0;
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);

    ret = mbedtls_sha256_starts_ret(&ctx, 0);
    if (ret)
        goto exit;

    ret = mbedtls_sha256_update_ret(&ctx, data, data_size);
    if (ret)
        goto exit;

    ret = mbedtls_sha256_finish_ret(&ctx, sha256);
    if (ret)
        goto exit;

exit:
    mbedtls_sha256_free(&ctx);
    return ret;
}

/**
 * decrypt the given data using current enclave's private key.
 * Used to receive encrypted data from another enclave.
 */
bool RSACrypto::decrypt(
    const uint8_t* encrypted_data,
    size_t encrypted_data_size,
    uint8_t* data,
    size_t* data_size) {
    bool ret = false;
    size_t output_size = 0;
    int res = 0;
    mbedtls_rsa_context* rsa_context;

    if (!m_initialized)
        goto exit;

    mbedtls_pk_rsa(m_pk_context)->len = encrypted_data_size;
    rsa_context = mbedtls_pk_rsa(m_pk_context);
    rsa_context->padding = MBEDTLS_RSA_PKCS_V21;
    rsa_context->hash_id = MBEDTLS_MD_SHA256;

    output_size = *data_size;
    res = mbedtls_rsa_rsaes_oaep_decrypt(
        rsa_context,
        mbedtls_ctr_drbg_random,
        &m_ctr_drbg_context,
        MBEDTLS_RSA_PRIVATE,
        NULL,
        0,
        &output_size,
        encrypted_data,
        data,
        output_size);
    if (res != 0) {
        std::cout << "RSA decryption failed with error: " << res << std::endl;
        goto exit;
    }
    *data_size = output_size;
    ret = true;

exit:
    return ret;
}

int RSACrypto::get_enclave_format_settings(const oe_uuid_t* format_id, buffer_t* format_settings) {
    uint8_t* format_settings_buffer = nullptr;
    size_t format_settings_size = 0;
    int ret = 1;

    // Intialize verifier to get enclave's format settings.
    if (oe_verifier_initialize() != OE_OK)
    {
        TRACE_ENCLAVE("oe_verifier_initialize failed");
        goto exit;
    }

    // Generate a format settings so that the enclave that receives this format
    // settings can attest this enclave.
    // Use the plugin.
    if (oe_verifier_get_format_settings(format_id, &format_settings_buffer, &format_settings_size) != OE_OK) {
        TRACE_ENCLAVE("oe_verifier_get_format_settings failed");
        goto exit;
    }

    if (format_settings_buffer && format_settings_size) {
        format_settings->buffer = (uint8_t*)malloc(format_settings_size);
        if (format_settings->buffer == nullptr) {
            ret = OE_OUT_OF_MEMORY;
            TRACE_ENCLAVE("copying format_settings failed, out of memory");
            goto exit;
        }
        memcpy(format_settings->buffer, format_settings_buffer, format_settings_size);
        format_settings->size = format_settings_size;
        oe_verifier_free_format_settings(format_settings_buffer);
    }
    else {
        format_settings->buffer = nullptr;
        format_settings->size = 0;
    }
    ret = 0;

exit:

    if (ret != 0)
        TRACE_ENCLAVE("get_enclave_format_settings failed.");
    return ret;
}

bool RSACrypto::generate_attestation_evidence(uint8_t** evidence, size_t* evidence_size) {
    bool ret = false;
    uint8_t hash[32];
    oe_result_t result = OE_OK;
    uint8_t* custom_claims_buffer = nullptr;
    size_t custom_claims_buffer_size = 0;
    char custom_claim_name[] = "Public key hash";

    buffer_t format_settings = {0};

    if (get_enclave_format_settings(&sgx_remote_uuid, &format_settings) != 0) {
        TRACE_ENCLAVE("Failed to get settings");
        return ret;
    }

    // The custom_claims[0].value will be filled with hash of public key later
    oe_claim_t custom_claims[1] = {{.name = custom_claim_name, .value = nullptr, .value_size = 0}};

    if (sha256(m_public_key, RSA_PUB_KEY_SIZE, hash) != 0) {
        TRACE_ENCLAVE("data hashing failed");
        return ret;
    }

    // Initialize attester and use the plugin.
    result = oe_attester_initialize();
    if (result != OE_OK) {
        TRACE_ENCLAVE("oe_attester_initialize failed.");
        return ret;
    }

    // serialize the custom claims, store hash of data in custom_claims[1].value
    custom_claims[0].value = hash;
    custom_claims[0].value_size = sizeof(hash);

    if (oe_serialize_custom_claims(custom_claims, 1, &custom_claims_buffer, &custom_claims_buffer_size) != OE_OK) {
        TRACE_ENCLAVE("oe_serialize_custom_claims failed.");
        return ret;
    }

    // Generate evidence based on the format selected by the attester.
    result = oe_get_evidence(
        &sgx_remote_uuid,
        0,
        custom_claims_buffer,
        custom_claims_buffer_size,
        format_settings.buffer,
        format_settings.size,
        evidence,
        evidence_size,
        nullptr,
        0);
    if (result != OE_OK) {
        TRACE_ENCLAVE("oe_get_evidence failed");
        return ret;
    }

    ret = true;
exit:
    return ret;
}

int RSACrypto::get_evidence(buffer_t* evidence) {

    uint8_t* evidence_buffer = nullptr;
    size_t evidence_size = 0;
    int ret = 1;

    if (!m_initialized ) {
        TRACE_ENCLAVE("ecall_dispatcher initialization failed.");
        goto exit;
    }

    // Generate evidence for the public key so that the enclave that
    // receives the key can attest this enclave.
    if (!generate_attestation_evidence(&evidence_buffer, &evidence_size)) {
        TRACE_ENCLAVE("get_evidence_with_public_key failed");
        goto exit;
    }

    evidence->buffer = (uint8_t*)malloc(evidence_size);
    if (evidence->buffer == nullptr) {
        ret = OE_OUT_OF_MEMORY;
        TRACE_ENCLAVE("copying evidence_buffer failed, out of memory");
        goto exit;
    }
    memcpy(evidence->buffer, evidence_buffer, evidence_size);
    evidence->size = evidence_size;
    oe_free_evidence(evidence_buffer);

    ret = 0;

exit:
    if (ret != 0) {
        if (evidence_buffer) {
            oe_free_evidence(evidence_buffer);
        }
        if (evidence) {
            free(evidence->buffer);
            evidence->size = 0;
        }
    }
    return ret;
}


uint8_t* RSACrypto::get_pub_key() {
    return m_public_key;
}