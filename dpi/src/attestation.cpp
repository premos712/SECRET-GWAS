#include "attestation.h"

/**
 * Helper function used to make the claim-finding process more convenient. Given
 * the claim name, claim list, and its size, returns the claim with that claim
 * name in the list.
 */
const oe_claim_t* _find_claim(const oe_claim_t* claims, size_t claims_size, const char* name) {
    for (size_t i = 0; i < claims_size; i++)
    {
        if (strcmp(claims[i].name, name) == 0)
            return &(claims[i]);
    }
    return nullptr;
}

// Compute the sha256 hash of given data.
int sha256(const uint8_t* data, size_t data_size, uint8_t sha256[32]) {
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

int Attestation::verify_evidence(buffer_t *evidence, const uint8_t* pubkey) {
    int ret = -1;
    uint8_t hash[32];
    oe_result_t result = OE_OK;
    oe_claim_t* claims = nullptr;
    size_t claims_length = 0;
    const oe_claim_t* claim;
    oe_claim_t* custom_claims = nullptr;
    size_t custom_claims_length = 0;

    unsigned char enclave_signer_id[32];
    size_t signer_id_size = sizeof(enclave_signer_id);
    

    // Intialize verifier to get enclave's format settings.
    if (oe_verifier_initialize() != OE_OK) {
        TRACE_ENCLAVE("oe_verifier_initialize failed");
        goto exit;
    }

    // Verify evidence (duh)
    result = oe_verify_evidence(&sgx_remote_uuid, evidence->buffer, evidence->size, nullptr, 0, nullptr, 0, &claims, &claims_length);
    if (result != OE_OK) {
        TRACE_ENCLAVE("oe_verify_evidence failed");
        goto exit;
    }

    // 2) validate the enclave identity's signer_id is the hash of the public
    // signing key that was used to sign an enclave. Check that the enclave was
    // signed by an trusted entity.
    if (oe_sgx_get_signer_id_from_public_key(ENCLAVE_PUBLIC_SIGNING_KEY, sizeof(ENCLAVE_PUBLIC_SIGNING_KEY), enclave_signer_id, &signer_id_size) != OE_OK) {
        goto exit;
    }

    // Validate the signer id.
    if ((claim = _find_claim(claims, claims_length, OE_CLAIM_SIGNER_ID)) == nullptr) {
        TRACE_ENCLAVE("Could not find claim.");
        goto exit;
    }

    if (claim->value_size != OE_SIGNER_ID_SIZE) {
        TRACE_ENCLAVE("signer_id size checking failed");
        goto exit;
    }

    if (memcmp(claim->value, enclave_signer_id, OE_SIGNER_ID_SIZE) != 0) {
        TRACE_ENCLAVE("Signer id does not match the one provided - did you compile the dpi with the correct 'enclave_signing_key.h' file?");
        goto exit;
    }

    // Check the enclave's product id.
    if ((claim = _find_claim(claims, claims_length, OE_CLAIM_PRODUCT_ID)) == nullptr) {
        TRACE_ENCLAVE("could not find claim");
        goto exit;
    };

    if (claim->value_size != OE_PRODUCT_ID_SIZE) {
        TRACE_ENCLAVE("product_id size checking failed");
        goto exit;
    }

    if (*(claim->value) != 1) {
        TRACE_ENCLAVE("product_id checking failed");
        goto exit;
    }

    // Check the enclave's security version.
    if ((claim = _find_claim(claims, claims_length, OE_CLAIM_SECURITY_VERSION)) == nullptr) {
        TRACE_ENCLAVE("could not find claim");
        goto exit;
    }

    if (claim->value_size != sizeof(uint32_t)) {
        TRACE_ENCLAVE("security_version size checking failed");
        goto exit;
    }

    if (*(claim->value) < 1) {
        TRACE_ENCLAVE("security_version checking failed");
        goto exit;
    }

    // 3) Validate the custom claims buffer
    //    Deserialize the custom claims buffer to custom claims list, then fetch
    //    the hash value of the data held in custom_claims[1].
    if ((claim = _find_claim(claims, claims_length, OE_CLAIM_CUSTOM_CLAIMS_BUFFER)) == nullptr) {
        TRACE_ENCLAVE("Could not find claim.");
        goto exit;
    }

    if (sha256(pubkey, RSA_PUB_KEY_SIZE, hash) != 0) {
        goto exit;
    }

    // deserialize the custom claims buffer
    if (oe_deserialize_custom_claims(claim->value, claim->value_size, &custom_claims, &custom_claims_length) != OE_OK) {
        TRACE_ENCLAVE("oe_deserialize_custom_claims failed.");
        goto exit;
    }
    
    if (custom_claims[0].value_size != sizeof(hash) || memcmp(custom_claims[0].value, hash, sizeof(hash)) != 0) {
        TRACE_ENCLAVE("hash mismatch");
        goto exit;
    }

    ret = 0;
exit:
    return ret;
}

    

// Phenotype::Phenotype(std::string _message, EnclaveNodeMessageType _mtype) 
//     : message(_message), mtype(_mtype){}

// void send_phenotype();