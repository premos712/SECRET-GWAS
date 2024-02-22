#ifndef _ATTEST_H_
#define _ATTEST_H_

#include <string>

#include <openenclave/attestation/attester.h>
#include <openenclave/attestation/custom_claims.h>
#include <openenclave/attestation/verifier.h>
#include <openenclave/bits/report.h>
#include <openenclave/bits/defs.h>
#include <openenclave/attestation/sgx/evidence.h>
#include <openenclave/attestation/sgx/report.h>
#include <openenclave/enclave.h>


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

#include "enclave_signing_key.h"
#include "parser.h"

inline void TRACE_ENCLAVE(const char* str) {
    std::cout << str << std::endl;
}

struct buffer_t {
    uint8_t *buffer;
    size_t size;
};

static const oe_uuid_t sgx_remote_uuid = {OE_FORMAT_UUID_SGX_ECDSA};

class Attestation {
    public:
        Attestation() {}
        static int verify_evidence(buffer_t *evidence, const uint8_t* pubkey);
};

#endif /* _PHENOTYPE_H*/
