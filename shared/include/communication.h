#ifndef _COMMUNICATION_H_
#define _COMMUNICATION_H_

#include <string>
#include <vector>

enum DPIMessageType {
  DPI_INFO,
  ENCLAVE_INFO,
  EVIDENCE,
  RSA_PUB_KEY,
  Y_AND_COV,
  DATA_REQUEST,
  DPI_SYNC,
  END_DPI
};

enum EnclaveNodeMessageType {
  GLOBAL_ID, 
  REGISTER,
  AES_KEY,
  PATIENT_COUNT,
  COVARIANT,
  Y_VAL,
  DATA,
  EOF_DATA,
  END_ENCLAVE
};

enum CoordinationServerMessageType {
  ENCLAVE_REGISTER,
  DPI_REGISTER,
  OUTPUT,
  EOF_OUTPUT
};

struct ConnectionInfo {
  std::string hostname;
  unsigned int port;
  unsigned int num_threads; // only for enclave node
};

struct DataBlock {
  std::string locus;
  std::string data;
};

struct DataBlockBatch {
  std::vector<DataBlock*> blocks_batch;
  int pos;
};

#endif