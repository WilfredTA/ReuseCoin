#include "blake2b.h"
#include "ckb_syscalls.h"
#include "common.h"


#include "blockchain.h"
#include "secp256k1_helper.h"
#include "secp256k1_lock.h"

/**
  This is the lock script for a wallet lock cell to pay script developers
  It enforces following rules:
    1. Payment is minimum rate
    2. Only one cell with this lock hash in input & output
    3. Type hash of this cell on input and out MUST be equal to
        type hash arg
    4. Output wallet must have UDT amount == udt rate

    TO DO:
      - make ckbyte compatible
**/

#define BLAKE2B_BLOCK_SIZE 32
#define MAX_SCRIPT_SIZE 32768
#define DATA_SIZE 32768
#define BALANCE_SIZE 16

#define ERROR_WALLET_QUANTITY -47
#define ERROR_AMOUNT -52

int has_signature(int *has_sig) {
  int ret;
  unsigned char temp[MAX_WITNESS_SIZE];

  /* Load witness of first input */
  uint64_t witness_len = MAX_WITNESS_SIZE;
  ret = ckb_load_witness(temp, &witness_len, 0, 0, CKB_SOURCE_GROUP_INPUT);

  if ((ret == CKB_INDEX_OUT_OF_BOUND) ||
      (ret == CKB_SUCCESS && witness_len == 0)) {
    *has_sig = 0;
    return CKB_SUCCESS;
  }

  if (ret != CKB_SUCCESS) {
    return ERROR_SYSCALL;
  }

  if (witness_len > MAX_WITNESS_SIZE) {
    return ERROR_WITNESS_SIZE;
  }

  /* load signature */
  mol_seg_t lock_bytes_seg;
  ret = extract_witness_lock(temp, witness_len, &lock_bytes_seg);
  if (ret != 0) {
    return ERROR_ENCODING;
  }

  *has_sig = lock_bytes_seg.size > 0;
  return CKB_SUCCESS;
}
int check_payment_unlock(uint64_t min_ckb_amount, uint128_t min_udt_amount, mol_seg_t* token_type) {
  // load this wallet lock hash
  // Ensure that only one wallet in inputs & outputs with this lock hash & type hash
  // Ensure that cell with this wallet hash has type hash == token_type in both in and outp

  // Ensure that out UDT > in UDT and that difference == min_udt_amount

  uint128_t udt_amount_in = 0;
  uint128_t udt_amount_out = 0;

  unsigned char lock_hash[BLAKE2B_BLOCK_SIZE];
  uint64_t len = BLAKE2B_BLOCK_SIZE;
  int ret = ckb_load_script_hash(lock_hash, &len, 0);
  if (ret != CKB_SUCCESS) {
    return ERROR_SYSCALL;
  }
  if (len > BLAKE2B_BLOCK_SIZE) {
    return ERROR_SCRIPT_TOO_LONG;
  }

  int i = 0;
  int wallet_in_index = 0;
  int udt_input_wallet_count = 0;
  while (1) {
    if (udt_input_wallet_count > 1) {
      return ERROR_WALLET_QUANTITY;
    }
    unsigned char type_hash[BLAKE2B_BLOCK_SIZE];
    uint64_t len = BLAKE2B_BLOCK_SIZE;

    int ret = ckb_checked_load_cell_by_field(type_hash, &len, 0, i,
              CKB_SOURCE_GROUP_INPUT, CKB_CELL_FIELD_TYPE_HASH);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (ret != CKB_SUCCESS) {
      return ret;
    }

    if (memcmp(type_hash, token_type->ptr, BLAKE2B_BLOCK_SIZE) == 0) {
      udt_input_wallet_count += 1;
      wallet_in_index = i;
    }

    i += 1;
  }

  if (udt_input_wallet_count != 1) {
    return ERROR_WALLET_QUANTITY;
  } else {
    unsigned char udt_amt[DATA_SIZE];
    uint64_t data_size = DATA_SIZE;
    int ret = ckb_load_cell_data(udt_amt, &data_size, 0, wallet_in_index, CKB_SOURCE_GROUP_INPUT);

    if (ret != CKB_SUCCESS) {
      return ret;
    }
    // Turn into molecule
    mol_seg_t udt_data;
    udt_data.ptr = (uint8_t *)udt_amt;
    udt_data.size = data_size;

    // Use molecule to read amount
    mol_seg_t amount = MolReader_UDTData_get_amount(&udt_data);
    mol_seg_t raw_amount = MolReader_Bytes_raw_bytes(&amount);
    uint128_t current_amt = *(uint128_t *)(raw_amount.ptr);
    udt_amount_in += current_amt;
  }

  i = 0;
  int udt_output_wallet_count = 0;
  int wallet_out_index = 0;
  while (1) {
    if (udt_output_wallet_count > 1) {
      return ERROR_WALLET_QUANTITY;
    }
    unsigned char type_hash[BLAKE2B_BLOCK_SIZE];
    uint64_t len = BLAKE2B_BLOCK_SIZE;

    int ret = ckb_checked_load_cell_by_field(type_hash, &len, 0, i,
              CKB_SOURCE_GROUP_OUTPUT, CKB_CELL_FIELD_TYPE_HASH);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (ret != CKB_SUCCESS) {
      return ret;
    }

    if (memcmp(type_hash, token_type->ptr, BLAKE2B_BLOCK_SIZE) == 0) {
      udt_output_wallet_count += 1;
      wallet_out_index = i;
    }
    i += 1;
  }

  if (udt_output_wallet_count != 1) {
    return ERROR_WALLET_QUANTITY;
  } else {
    unsigned char udt_amt[DATA_SIZE];
    uint64_t data_size = DATA_SIZE;
    int ret = ckb_load_cell_data(udt_amt, &data_size, 0, wallet_out_index, CKB_SOURCE_GROUP_OUTPUT);
    if (ret != CKB_SUCCESS) {
      return ret;
    }
    // Turn into molecule
    mol_seg_t udt_data;
    udt_data.ptr = (uint8_t *)udt_amt;
    udt_data.size = data_size;

    // Use molecule to read amount
    mol_seg_t amount = MolReader_UDTData_get_amount(&udt_data);
    mol_seg_t raw_amount = MolReader_Bytes_raw_bytes(&amount);
    uint128_t current_amt = *(uint128_t *)(raw_amount.ptr);
    udt_amount_out += current_amt;
  }

  if ((udt_amount_out > udt_amount_in) &&
  ((udt_amount_out - udt_amount_in) >= min_udt_amount)) {
    return CKB_SUCCESS;
  }
  return ERROR_AMOUNT;

}


int main() {

  // load args

  unsigned char script[MAX_SCRIPT_SIZE];
  uint64_t len = MAX_SCRIPT_SIZE;

  int ret = ckb_load_script(script, &len, 0);
  if (ret != CKB_SUCCESS) {
    return ERROR_SYSCALL;
  }
  if (len > SCRIPT_SIZE) {
    return ERROR_SCRIPT_TOO_LONG;
  }

  mol_seg_t script_seg;
  script_seg.ptr = (uint8_t *)script;
  script_seg.size = len;

  if (MolReader_Script_verify(&script_seg, false) != MOL_OK) {
    return ERROR_ENCODING;
  }

  mol_seg_t args_seg = MolReader_Script_get_args(&script_seg);
  mol_seg_t raw_args = MolReader_Bytes_raw_bytes(&args_seg);

  if (MolReader_ReuseCoinArgs_verify(&raw_args, false) != MOL_OK) {
    return ERROR_ENCODING;
  }

  mol_seg_t pubkey_hash = MolReader_ReuseCoinWalletArgs_get_pubkey_hash(&raw_args);
  mol_seg_t ckb_rate = MolReader_ReuseCoinWalletArgs_get_ckb_rate(&raw_args);
  mol_seg_t udt_rate = MolReader_ReuseCoinWalletArgs_get_udt_rate(&raw_args);
  mol_seg_t token_type = MolReader_ReuseCoinWalletArgs_get_token_type(&raw_args);

  uint64_t ckb_pay_amt = *(uint64_t *)(MolReader_Bytes_raw_bytes(&ckb_rate)).ptr;
  uint128_t udt_pay_amt = *(uint128_t *)(MolReader_Bytes_raw_bytes(&udt_rate)).ptr;
  int has_sig;
  int check_sig_ret = has_signature(&has_sig);
  if (check_sig_ret != CKB_SUCCESS) {
    return check_sig_ret;
  }
  if (has_sig) {
    return verify_secp256k1_blake160_sighash_all(pubkey_hash.ptr);
  } else {
    return check_payment_unlock(ckb_pay_amt, udt_pay_amt, &token_type);
  }
}
