for count in range(100000, 2100000, 100000):
    for cov in range(1, 17, 1):
        with open(f'../dpi/configs/dpi_config-{count}.json', 'w') as f:
            f.write('{\n')
            f.write('\t"dpi_name": "dpi1",\n')
            f.write(f'\t"dpi_bind_port": 18601,\n')
            f.write(f'\t"allele_file": "dpi_data/generated_alleles_{count}-125000.tsv",\n')
            f.write('\t"coordination_server_info": { "hostname": "localhost", "port": 6401 }\n')
            f.write('}')

# {
#     "dpi_name": "dpi1",
#     "dpi_bind_port": 8601,
#     "allele_file": "dpi_data/generated_alleles_2500.tsv",
#     "coordination_server_info": {
#         "hostname": "102.37.153.209",
#         "port": 6401
#     }
# }