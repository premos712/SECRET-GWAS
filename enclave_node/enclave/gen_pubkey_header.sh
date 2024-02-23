#!/usr/bin/env bash

# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

destfile="../../dpi/include/enclave_signing_key.h"
pubkey_file="keys/public.pem"

cat > "$destfile" << EOF
// Copyright (c) Open Enclave SDK contributors.
// Licensed under the MIT License.

#ifndef SAMPLES_ATTESTATION_PUBKEY_H
#define SAMPLES_ATTESTATION_PUBKEY_H

EOF

printf 'static const char ENCLAVE_PUBLIC_SIGNING_KEY[] =' >> "$destfile"
while IFS="" read -r p || [ -n "$p" ]
do
    # Sometimes openssl can insert carriage returns into the PEM files. Let's remove those!
    CR=$(printf "\r")
    p=$(echo "$p" | tr -d "$CR")
    printf '\n    \"%s\\n\"' "$p" >> "$destfile"
done < "$pubkey_file"
printf ';\n' >> "$destfile"

cat >> "$destfile" << EOF

#endif /* SAMPLES_ATTESTATION_PUBKEY_H */
EOF