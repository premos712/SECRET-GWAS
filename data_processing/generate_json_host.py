for count in range(100000, 2100000, 100000):
    for cov in range(1, 17, 1):
        with open(f'../enclave_node/host/configs/enclave_node_config-{cov}-{count}.json', 'w') as f:
            f.write('{\n')
            f.write('\t"enclave_node_bind_port": 16701,\n')
            cov_str = ""
            for i in range(cov):
                cov_str += f'"{i+1}-{count}", '
            f.write(f'\t"covariants": [{cov_str[:-2]}],\n')
            f.write('\t"institutions": ["dpi1"],\n')
            f.write(f'\t"y_val_name": "disease-{count}",\n')
            f.write('\t"analysis_type": "linear",\n')
            f.write('\t"enclave_path": "../enclave/gwasenc.signed",\n')
            f.write('\t"coordination_server_info": { "hostname": "localhost", "port": 6401 }\n')
            f.write('}')
