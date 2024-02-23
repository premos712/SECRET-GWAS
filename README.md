# SECRET-GWAS: Confidential Computing for Population-Scale GWAS

## Overview

* [Introduction](#introduction)
* [Installation](#installation)
* [Usage](#usage)
* [Acknowledgements](#acknowledgements)
* [Limitations](#limitations)
* [License](#license)


## Introduction
*SECRET-GWAS* (Scalable Efficient Confidential and privacy-Respecting Environment for Trusted GWAS) is a tool that:
- Parses Hail format files.
- Performs **blazing fast GWAS** using linear or logistic regression.
- Scales to **hundreds** of independent machines and processor cores.
- **Protects against side-channels** like Spectre and control flow/data access pattern leakages.

This allows for collaborative GWAS at a population-scale (millions of patients) that can run in seconds/minutes. With *SECRET-GWAS*, running a query is quick and painless.

This project is currently under active development and welcomes feedback, issues, and suggestions.

## Installation

SECRET-GWAS is developed for Linux and has been tested on Ubuntu 18.04 and 20.04. Use the appropriate installation script:

```
# If using Ubuntu 18.04.
> ./install-18.04.sh

# If using Ubuntu 20.04.
> ./install-20.04.sh
```

Additionally, if you wish to run the demo you will need to install Hail.

```
# For the demo, install Hail and Hail dependencies.
> ./hail_demo/install_reqs.sh
```

## Usage

### Preface
After running the installation script, all binaries should be compiled. If for any reason you ever need to recompile make sure to add the following to your source:

```
> source /opt/openenclave/share/openenclave/openenclaverc
```

Additionally, when the Enclave Node (EN) binary is compiled it will generate a header for the Data Providing Institution (DPI) to use for attestation. If you are deploying across multiple machines make sure that you use the same signed binary for every EN instance and distribute the public key to each DPI.

### Running SECRET-GWAS Locally
We provide example configuration files to get you started. To run an example follow the below steps:

0. The following example is easier if you use three seperate bash shells. However it can be done in one by running each command as a background process by adding a `&` to the end of each command
1. Go to the coordination server directory and start it up
```
> cd coordination_server/ && make demo
# or
> cd coordination_server/ && ./bin/coordination_server configs/coordination_server_configs.json
```
You should see the message
```
Running on port 16401
```
2. Go to the EN directory and start it up
```
> cd enclave_node/host/ && make demo
# or
> cd enclave_node/ && ./gwashost configs/enclave_node_config-demo.json
```
You should see the message
```
**RUNNING REGRESSION**

RSA pubkey/evidence generated and set
enclave running on 1 dpis
```

3. Go to the DPI directory and start it up
```
> cd dpi/ && make demo
# or
> cd dpi/ && ./bin/dpi configs/dpi_config-demo.json
```
You should see the message
```
./bin/dpi configs/dpi_config-demo.json
```

Once both the EN and DPI communicate with the CS, the CS will tell all entities how to communicate with eachother, the EN and DPI will do attestation and share keys with eachother, then the GWAS will happen. The final result will be sent to the CS and then all entities will shut down.

### Running SECRET-GWAS Remotely
Running the system with remote servers follows identical steps, but the configurations must be tweaked. All ENs and DPIs must use a configuration file that specifies the IP address of the CS. This is the only IP address that must be hardcoded in a configuration file.

There are many more options to change in the configuration file such as input files, number of ENs/DPIs, etc.


## Hail Demo

To run the Hail demo, first see the installation instructions to make sure you have all the prerequisites installed.

Enter the demo directory

```
> cd hail_demo/
```

Active the Python virtual environment

```
> source env/bin/activate
```

You can now use the demo. First run the original Hail demo

```
> python3 hail_gwas.py
```

The output will be written to `Hail_result.vcf`. To run the Hail/*SECRET-GWAS* equivalent that performs the association using *SECRET-GWAS* use the second demo script


```
> python3 SECRET_gwas.py
```

The output will be written to `SECRET_results.vcf`. This can be compared to the original Hail demo output file, as can the two scripts. This is not a true integration into Hail, just a demonstration of how *SECRET-GWAS* parses Hail input files and creates similar output files.

## Acknowledgements
- Inspiration for several design decisions were taken from <a href="https://hail.is/" target = “_blank”>Hail</a>. We also use Hail for the filtering and QC in our GWAS pipeline.
- The data we used for testing is upsampled from a <a href="https://www.internationalgenome.org/" target = “_blank”>1000 Genomes Project</a> snippet provided by <a href="https://hail.is/docs/0.2/tutorials/01-genome-wide-association-study.html" target = “_blank”>Hail</a>.
- We use several third-party libraries for synchronization free data structures, json parsing, and thread pooling. All can be found 
<a href="./shared/third_party" target=“_blank”>here</a>.

## Limitations
The current version of SECRET-GWAS does not yet implement:
- Imputation methods other than the one used by Hail (average value).
- Collaborative GWAS pipeline stages aside from genetic association. Hail must be used locally for filtering, QC, PCA, etc.

## License
This project is covered under the <a href="LICENSE">MIT</a> license.