// This file is an implementation of the UDT definition for a token based on the SUDT Standard
// Right now it is only a POC. Not tested for production.
// To do:
// Protect from arithmetic overflow
#include "blockchain.h"
#include "ckb_syscalls.h"

#define GOV_SCRIPT_HASH_SIZE 32
#define SCRIPT_SIZE 32768

#define ERROR_ARGUMENTS_LEN -1
#define ERROR_ENCODING -2
#define ERROR_SYSCALL -3
#define ERROR_SCRIPT_TOO_LONG -21
#define ERROR_OVERFLOWING -51
#define ERROR_AMOUNT -52
#define DATA_SIZE 16
typedef unsigned __int128 uint128_t;



int verify_udt_input_cell(int idx) {
  //---------------------- LOAD IN RAW CELL DATA ---------------
    uint128_t data;
    uint64_t data_size = DATA_SIZE;
    int data_load_ret = ckb_load_cell_data((uint8_t*)&data, &data_size, 0, idx, CKB_SOURCE_GROUP_INPUT);
    if (data_load_ret != CKB_SUCCESS) {
      return data_load_ret;
    }
    if (data_size != DATA_SIZE) {
      return ERROR_ENCODING;
    }

    return CKB_SUCCESS;
}



int verify_udt_output_cell(int idx) {
  //---------------------- LOAD IN RAW CELL DATA ---------------
    uint128_t current_amt;
    uint64_t data_size = DATA_SIZE;
    int data_load_ret = ckb_load_cell_data((uint8_t*)&current_amt, &data_size, 0, idx, CKB_SOURCE_GROUP_OUTPUT);
    if (data_load_ret != CKB_SUCCESS) {
      return data_load_ret;
    }

    if (data_size != DATA_SIZE) {
      ckb_debug("NOT THE RIGHT DATA SIZE");
      return ERROR_ENCODING;
    }
    if (current_amt < 1000) {
      ckb_debug("AMOUNT < 1000");
    } else if (current_amt == 1000) {
      ckb_debug("AMOUNT == 1000");
    } else if (current_amt > 1000) {
      ckb_debug("AMOUNT > 1000");
    }

    //---------------------- VALIDATE AGAINST SCHEMA ---------------
    // if (MolReader_UDTData_verify(&udt_data, false) != MOL_OK) {
    //   ckb_debug("ERROR Encoding of UDT Instance in outputs\n");
    //   return ERROR_ENCODING;
    // }
    return CKB_SUCCESS;

}

int verify_all_udt() {
  size_t i = 0;
  while(1) {
    int ret = verify_udt_input_cell(i);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }

    if (ret != CKB_SUCCESS) {
      return ret;
    }
    i += 1;
  }

  i = 0;
  while(1) {
    int ret = verify_udt_output_cell(i);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }

    if (ret != CKB_SUCCESS) {
      return ret;
    }
    i += 1;
  }

  return CKB_SUCCESS;

}

int verify_udt_usage() {
  uint128_t input_amount = 0;
  size_t i = 0;
  while(1) {
    // Load in raw data
    uint128_t udt_amt;
    uint64_t data_size = DATA_SIZE;
    int ret = ckb_load_cell_data((uint8_t*)&udt_amt, &data_size, 0, i, CKB_SOURCE_GROUP_INPUT);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (ret != CKB_SUCCESS) {
      return ret;
    }

    // Add cell's amount to total input amount
    input_amount += udt_amt;

    i += 1;
  }

  uint128_t output_amount = 0;
  i = 0;
  while(1) {
    uint128_t udt_amt;
    uint64_t data_size = DATA_SIZE;
    int ret = ckb_load_cell_data((uint8_t*)&udt_amt, &data_size, 0, i, CKB_SOURCE_GROUP_OUTPUT);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (ret != CKB_SUCCESS) {
      return ret;
    }

    output_amount += udt_amt;

    i += 1;
  }

  if (output_amount > input_amount) {
    return ERROR_AMOUNT;
  }

  return CKB_SUCCESS;
}

int main() {
  // Verify the first arg as a 32byte script hash
  // 1. Load Raw Cell Data
  ckb_debug("UDT DEFINITION EXECUTING\n");
  unsigned char cell_script[SCRIPT_SIZE];
  uint64_t script_size = SCRIPT_SIZE;
  int script_load_ret = ckb_load_script(cell_script, &script_size, 0);
  if (script_load_ret != CKB_SUCCESS) {
    return script_load_ret;
  }

  // Turn it into molecule
  mol_seg_t mol_script;
  mol_script.ptr = (uint8_t *)cell_script;
  mol_script.size = script_size;

  mol_seg_t script_args = MolReader_Script_get_args(&mol_script);
  mol_seg_t raw_args = MolReader_Bytes_raw_bytes(&script_args);

  // Validate that it is a 32 byte value
  if (raw_args.size != GOV_SCRIPT_HASH_SIZE) {
    return ERROR_ARGUMENTS_LEN;
  }


  if (verify_all_udt() != CKB_SUCCESS) {
    return ERROR_AMOUNT;
  }



  int permissions_enabled = 0;
  size_t i = 0;
  while (1) {
    unsigned char input_script_hash[GOV_SCRIPT_HASH_SIZE];
    uint64_t script_size = GOV_SCRIPT_HASH_SIZE;
    int load_lock_ret = ckb_checked_load_cell_by_field(input_script_hash,
      &script_size, 0, i, CKB_SOURCE_INPUT, CKB_CELL_FIELD_LOCK_HASH);

    if (load_lock_ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }

    if (load_lock_ret != CKB_SUCCESS) {
      return load_lock_ret;
    }

    if (memcmp(raw_args.ptr, input_script_hash, GOV_SCRIPT_HASH_SIZE) == 0) {
      permissions_enabled = 1;
      break;
    }
    i += 1;
  }

  if (permissions_enabled != 1) {
    return verify_udt_usage();
  } else {
    return CKB_SUCCESS;
  }

}
