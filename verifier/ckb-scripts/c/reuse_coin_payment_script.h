// This library should be included in any script to make it
// reuse coin compatible. All you have to do is include it and run reuse_coin_verify()
// at the beginning of your script. Right now, you have to include this in your script entirely.
// In the future, this will be deployed on-chain and all you will need to do is use the simple reuse_coin_plugin
// that will dynamically load this script at execution time from the dep cells in a tx.


// TO DO
// Safe arithmetic protect from overflow
// Make shared library
// Make compatible w/ native CKBytes
#include "blockchain.h"
#include "ckb_syscalls.h"

#define MAX_SCRIPT_SIZE 32768
#define HASH_SIZE 32
#define BALANCE_SIZE 16


#define ERROR_ARGS_ENCODING -51
#define ERROR_INSUFFICIENT_PAYMENT -52
#define ERROR_CELL_WALLET -53

typedef unsigned __int128 uint128_t;
// Expected args:
// The args are expected to be serialized in molecule according to ReuseCoinArgs schema.
// The reuse coin specific args are the following:
// 1. 32 byte lock hash of the cell wallet that your funds will be transferred to
int reuse_coin_verify() {

  // First, load args and verify
  unsigned char script[MAX_SCRIPT_SIZE];
  uint64_t script_size = MAX_SCRIPT_SIZE;
  int script_load_ret = ckb_load_script(script, &script_size, 0);

  if (script_load_ret != CKB_SUCCESS) {
    return script_load_ret;
  }

  mol_seg_t script_seg;
  script_seg.ptr = (uint8_t *)script;
  script_seg.size = script_size;

  mol_seg_t script_args = MolReader_Script_get_args(&script_seg);
  mol_seg_t raw_args = MolReader_Bytes_raw_bytes(&script_args);

  if (raw_args.size < HASH_SIZE) {
    return ERROR_ENCODING;
  }

  int found_in_input = 0;
  int found_in_output = 0;

  int i = 0;
  while (1) {
    unsigned char temp_hash[HASH_SIZE];
    uint64_t lock_hash_size = HASH_SIZE;

    int lock_hash_ret = ckb_load_cell_by_field(temp_hash, &lock_hash_size, 0, i,
      CKB_SOURCE_INPUT, CKB_CELL_FIELD_LOCK_HASH);
    if (lock_hash_ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (lock_hash_ret  != CKB_SUCCESS) {
      return lock_hash_ret ;
    }

    if (memcmp(temp_hash, raw_args.ptr, HASH_SIZE) == 0) {
         found_in_input += 1;
      }
    i++;
  }

  i = 0;

  while (1) {
    unsigned char temp_hash[HASH_SIZE];
    uint64_t lock_hash_size = HASH_SIZE;

    int lock_hash_ret = ckb_load_cell_by_field(temp_hash, &lock_hash_size, 0, i,
      CKB_SOURCE_OUTPUT, CKB_CELL_FIELD_LOCK_HASH);

    if (lock_hash_ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (lock_hash_ret != CKB_SUCCESS) {
      return lock_hash_ret;
    }

    if (memcmp(temp_hash, raw_args.ptr, HASH_SIZE) == 0) {
          found_in_output += 1;
      }

    i++;
  }

  if (found_in_input == 1 && found_in_output == 1) {
    return CKB_SUCCESS;
  } else {
    return ERROR_CELL_WALLET;
  }

}
