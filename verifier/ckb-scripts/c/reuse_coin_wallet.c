#include "blake2b.h"
#include "ckb_syscalls.h"
#include "common.h"


#include "blockchain.h"
#include "secp256k1_helper.h"
#include "secp256k1_lock.h"

#define BLAKE2B_BLOCK_SIZE 32
#define MAX_SCRIPT_SIZE 32768
#define DATA_SIZE 16
#define BALANCE_SIZE 16
#define CAPACITY_SIZE 8
#define MIN_ARGS_LENGTH (BLAKE160_SIZE + CAPACITY_SIZE + BALANCE_SIZE + BLAKE2B_BLOCK_SIZE)
#define MAX_ARGS_LENGTH (MIN_ARGS_LENGTH + BLAKE2B_BLOCK_SIZE)

#define ERROR_WALLET_QUANTITY -47
#define ERROR_AMOUNT -48
#define ERROR_UNIQUE_SCRIPT_VIOLATION -49
#define ERROR_UNIQUE_SCRIPT_MISSING_TYPE_FIELD -50
#define ERROR_UNIQUE_SCRIPT_MISMATCH -51
#define ERROR_CELL_DEP_LOAD -52

#define ERROR_WALLET_CELL_MISSING_TYPE -53
#define ERROR_WALLET_EXPECTS_DIFFERENT_TOKEN_TYPE -54
#define ERROR_TOO_MANY_WALLETS_OF_SAME_TYPE -55
#define ERROR_NO_INPUT_WALLET_FOUND -56
#define ERROR_NO_OUTPUT_WALLET_FOUND -57
#define ERROR_WALLET_UNLOCK -58
typedef unsigned __int128 uint128_t;

int get_udt_amount(unsigned char *data, uint64_t data_len, uint128_t* amount) {
  mol_seg_t udt_data;
  udt_data.ptr = (uint8_t *)data;
  udt_data.size = data_len;

  // Use molecule to read amount
  if (MolReader_UDTData_verify(&udt_data, false) != MOL_OK) {
    ckb_debug("ERROR IN UDT DATA ENCODING");
    return ERROR_ENCODING;
  }
  mol_seg_t amount_seg = MolReader_UDTData_get_amount(&udt_data);
  //mol_seg_t raw_amount = MolReader_Bytes_raw_bytes(&amount_seg);
  uint128_t current_amt = *(uint128_t *)(amount_seg.ptr);
  *amount += current_amt;
  return CKB_SUCCESS;
}

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
    ckb_debug("ERROR ENCODING IN WITNESS LOCK");
    return ERROR_ENCODING;
  }

  *has_sig = lock_bytes_seg.size > 0;
  return CKB_SUCCESS;
}

int check_deps(int uniq_script, unsigned char *script_type_hash, uint128_t* expected_udt_pay, unsigned char *lock_hash, uint128_t udt_rate) {

  int i = 0;
  int deps_with_lock_hash = 0;

  while (1) {
    if (uniq_script && deps_with_lock_hash > 1) {
      return ERROR_UNIQUE_SCRIPT_VIOLATION;
    }
    unsigned char dep_lock_hash[BLAKE2B_BLOCK_SIZE];
    uint64_t len = BLAKE2B_BLOCK_SIZE;
    int ret = ckb_load_cell_by_field(dep_lock_hash, &len, 0, i,
      CKB_SOURCE_CELL_DEP, CKB_CELL_FIELD_LOCK_HASH);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (ret != CKB_SUCCESS) {
      return ERROR_CELL_DEP_LOAD;
    }
    if (memcmp(lock_hash, dep_lock_hash, BLAKE2B_BLOCK_SIZE) == 0) {
      *expected_udt_pay += udt_rate;
      deps_with_lock_hash++;

      if (uniq_script) {

        unsigned char dep_type_hash[BLAKE2B_BLOCK_SIZE];
        uint64_t hash_len = BLAKE2B_BLOCK_SIZE;
        int type_ret = ckb_load_cell_by_field(dep_lock_hash, &hash_len, 0, i,
          CKB_SOURCE_CELL_DEP, CKB_CELL_FIELD_TYPE_HASH);

        if (type_ret == CKB_ITEM_MISSING) {
          ckb_debug("Unique script requires type hash on reusable script!");
          return ERROR_UNIQUE_SCRIPT_MISSING_TYPE_FIELD;
        }

        if (type_ret != CKB_SUCCESS) {
          return ERROR_SYSCALL;
        }

        if (memcmp(script_type_hash, dep_type_hash,
          BLAKE2B_BLOCK_SIZE) != 0) {
          return ERROR_UNIQUE_SCRIPT_MISMATCH;
        }
      }
    }
    i++;
  }
  return CKB_SUCCESS;
}

int check_inputs(uint128_t *input_udt_balance, uint64_t *input_capacity, int *wallet_count, unsigned char *token_type) {
  int i = 0;
  while (1) {

    unsigned char type_hash[BLAKE2B_BLOCK_SIZE];
    uint64_t len = BLAKE2B_BLOCK_SIZE;
    int ret = ckb_load_cell_by_field(type_hash, &len, 0, i,
      CKB_SOURCE_GROUP_INPUT, CKB_CELL_FIELD_TYPE_HASH);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }

    if (ret == CKB_ITEM_MISSING) {
      return ERROR_WALLET_CELL_MISSING_TYPE;
    }

    if (ret != CKB_SUCCESS) {
      return ERROR_SYSCALL;
    }

    if (memcmp(type_hash, token_type, BLAKE2B_BLOCK_SIZE) != 0) {
      return ERROR_WALLET_EXPECTS_DIFFERENT_TOKEN_TYPE;
    } else {
      *wallet_count += 1;
      if (*wallet_count > 1) {
        ckb_debug("Too many input wallets of same type");
        return ERROR_TOO_MANY_WALLETS_OF_SAME_TYPE;
      }
      // get data field in wallet and add to input_udt balance
      ckb_debug("ADDING UDT AMOUNT FROM INPUT WALLET");
      uint128_t udt_amt;
      uint64_t data_len = DATA_SIZE;
      int data_ret = ckb_load_cell_data((uint8_t *)&udt_amt, &data_len, 0, i, CKB_SOURCE_GROUP_INPUT);
      if (data_ret != CKB_SUCCESS) {
        return ERROR_SYSCALL;
      }
      if (data_len != DATA_SIZE) {
        ckb_debug("ERROR IN UDT AMOUNT ON INPUT WALLET");
        return ERROR_ENCODING;
      }

      *input_udt_balance = udt_amt;
      if (*input_udt_balance == 1000) {
        ckb_debug("INPUT BALANCE IS 1000");
      } else if (*input_udt_balance < 1000) {
        ckb_debug("INPUT BALANCE < 1000");
      } else if (*input_udt_balance > 1000){
        ckb_debug("INPUT BALANCE > 1000");
      }

      // get input wallet's capacity
      uint64_t ckbytes;
      uint64_t capacity_len = CAPACITY_SIZE;
      int ckbyte_load = ckb_checked_load_cell_by_field((uint8_t *)&ckbytes, &capacity_len,
        0, i, CKB_SOURCE_GROUP_INPUT, CKB_CELL_FIELD_CAPACITY);

      if (ckbyte_load != CKB_SUCCESS) {
        ckb_debug("ERROR loading input wallet capacity");
        return ERROR_SYSCALL;
      }
      *input_capacity = ckbytes;
    }
    i++;
  }
  return CKB_SUCCESS;
}

int check_outputs(uint128_t *output_udt_balance, uint64_t *output_capacity, int *wallet_count, unsigned char *token_type, unsigned char *lock_hash) {
  int i = 0;
  while (1) {
    // find output w/ this lock_hash
    // if its type hash != token_type, return error
    unsigned char script[BLAKE2B_BLOCK_SIZE];
    uint64_t len = BLAKE2B_BLOCK_SIZE;
    int ret = ckb_load_cell_by_field(script, &len, 0, i, CKB_SOURCE_OUTPUT, CKB_CELL_FIELD_LOCK_HASH);
    if (ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (ret != CKB_SUCCESS) {
      ckb_debug("error loading output lock hash in wallet script");
      return ERROR_SYSCALL;
    }
    if (memcmp(script, lock_hash, BLAKE2B_BLOCK_SIZE) != 0) {
      i++;
      continue;
    }

    unsigned char type_script[BLAKE2B_BLOCK_SIZE];
    uint64_t type_len = BLAKE2B_BLOCK_SIZE;
    int type_ret = ckb_load_cell_by_field(type_script, &type_len, 0, i, CKB_SOURCE_OUTPUT, CKB_CELL_FIELD_TYPE_HASH);
    if (type_ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (type_ret == CKB_ITEM_MISSING) {
      ckb_debug("OUTPUT WALLET EXPECTED TYPE SCRIPT and did not find one");
      return ERROR_WALLET_CELL_MISSING_TYPE;
    }
    if (type_ret != CKB_SUCCESS) {
      ckb_debug("Unknown error in loading output wallet's type hash");
      return type_ret;
    }
    if (memcmp(type_script, token_type, BLAKE2B_BLOCK_SIZE) != 0){
      ckb_debug("Mismatch between output wallet's expected token type and actual token type");
      return ERROR_WALLET_EXPECTS_DIFFERENT_TOKEN_TYPE;
    }
    *wallet_count += 1;
    if (*wallet_count > 1) {
      ckb_debug("Too many wallets in output. You can only use 1 wallet with same lock hash");
      return ERROR_TOO_MANY_WALLETS_OF_SAME_TYPE;
    }
    // Get udt amount in wallet cell
    uint128_t udt_amt;
    uint64_t data_len = DATA_SIZE;
    int data_ret = ckb_load_cell_data((uint8_t*)&udt_amt, &data_len, 0, i, CKB_SOURCE_OUTPUT);
    if (data_ret != CKB_SUCCESS) {
      return ERROR_SYSCALL;
    }
    ckb_debug("ADDING UDT AMOUNT FROM OUTPUT WALLET");

    *output_udt_balance = udt_amt;

    if (*output_udt_balance == 1000) {
      ckb_debug("output BALANCE IS 1000");
    } else if (*output_udt_balance < 1000) {
      ckb_debug("output BALANCE < 1000");
    } else if (*output_udt_balance > 1000){
      ckb_debug("output BALANCE > 1000");
    }

    // Get ckbytes in wallet cell
    uint64_t ckbytes;
    uint64_t capacity_len = CAPACITY_SIZE;
    int ckbyte_load = ckb_checked_load_cell_by_field((uint8_t *)&ckbytes, &capacity_len,
      0, i, CKB_SOURCE_OUTPUT, CKB_CELL_FIELD_CAPACITY);

    if (ckbyte_load != CKB_SUCCESS) {
      ckb_debug("ERROR loading output wallet capacity");
      return ERROR_SYSCALL;
    }
    *output_capacity = ckbytes;
    i++;
  }
  return CKB_SUCCESS;
}

int main () {
  // int has_unique_script;
  ckb_debug("REUSE COIN WALLET LOCK SCRIPT EXECUTING\n");
  int has_sig;
  int uniq_mode = 0;


  // Load script hash
  unsigned char lock_hash[BLAKE2B_BLOCK_SIZE];
  uint64_t hash_size = BLAKE2B_BLOCK_SIZE;
  int load_hash_ret = ckb_load_script_hash(lock_hash, &hash_size, 0);
  if (load_hash_ret != CKB_SUCCESS) {
    return load_hash_ret;
  }

  // Load script
  unsigned char script[MAX_SCRIPT_SIZE];
  uint64_t script_len = MAX_SCRIPT_SIZE;
  int load_script_ret = ckb_load_script(script, &script_len, 0);
  if (load_script_ret != CKB_SUCCESS) {
    return ERROR_SYSCALL;
  }

  mol_seg_t script_seg;
  script_seg.ptr = (uint8_t *)script;
  script_seg.size = script_len;

  if (MolReader_Script_verify(&script_seg, false) != MOL_OK) {
    ckb_debug("ERROR IN SCRIPT ENCODING OF CELL WALLET LOCK");
    return ERROR_ENCODING;
  }

  mol_seg_t args_seg = MolReader_Script_get_args(&script_seg);
  mol_seg_t raw_args = MolReader_Bytes_raw_bytes(&args_seg);

  if (raw_args.size < MIN_ARGS_LENGTH) {
    ckb_debug("ARGS IN WALLET TOO SHORT");
    return ERROR_ARGUMENTS_LEN;
  }

  if (raw_args.size > MAX_ARGS_LENGTH) {
    ckb_debug("ARGS IN WALLET TOO LONG");
    return ERROR_ARGUMENTS_LEN;
  }

  if (raw_args.size != MIN_ARGS_LENGTH &&
      raw_args.size != MAX_ARGS_LENGTH) {
        ckb_debug("ARGS IN WALLET CAN ONLY BE 77 or 109 bytes long");
        return ERROR_ARGUMENTS_LEN;
      }
  unsigned char pubkey_hash[BLAKE160_SIZE];
  unsigned char ckb_rate[CAPACITY_SIZE];
  unsigned char udt_rate[BALANCE_SIZE];
  unsigned char token_type[BLAKE2B_BLOCK_SIZE];
  unsigned char reusable_script_type_hash[BLAKE2B_BLOCK_SIZE];

  memcpy(pubkey_hash, raw_args.ptr, BLAKE160_SIZE);
  memcpy(ckb_rate, &raw_args.ptr[BLAKE160_SIZE], CAPACITY_SIZE);
  memcpy(udt_rate, &raw_args.ptr[BLAKE160_SIZE + CAPACITY_SIZE], BALANCE_SIZE);
  memcpy(token_type, &raw_args.ptr[BLAKE160_SIZE + CAPACITY_SIZE + BALANCE_SIZE], BLAKE2B_BLOCK_SIZE);

  if (raw_args.size == MAX_ARGS_LENGTH) {
    uniq_mode = 1;
    memcpy(reusable_script_type_hash, &raw_args.ptr[MIN_ARGS_LENGTH], BLAKE2B_BLOCK_SIZE);
  }



  uint64_t ckb_pay_amt = *(uint64_t *)ckb_rate;
  uint128_t udt_pay_amt = *(uint128_t *)udt_rate;




  int sig_check_ret = has_signature(&has_sig);

  if (sig_check_ret != CKB_SUCCESS) {
    return sig_check_ret;
  }


  uint128_t expected_udt_pay = udt_pay_amt;
  uint128_t input_udt_balance = 0;
  uint128_t output_udt_balance = 0;
  uint64_t input_wallet_capacity = 0;
  uint64_t output_wallet_capacity = 0;
  int input_wallet_count = 0;
  int output_wallet_count = 0;


    // loop through deps
      //  if has_unique_script, ensure that there is only one dep cell whose arg == this lock hash
      // and that dep cell's type script matches reusable script type hash
      // else
      // add expected amount for each dep cell found with this lock hash
    int dep_check = check_deps(uniq_mode, reusable_script_type_hash, &expected_udt_pay, lock_hash, udt_pay_amt);
    if (dep_check != CKB_SUCCESS) {
      return dep_check;
    }

    ckb_debug("AFTER DEP CHECK");
    // loop through inputs
    // if wallet_count > 1 return error
    // if cell with this token_type == type-hash is found in this script group
    // record udt_in_amt and increment wallet count
    // record capacity amount
    ckb_debug("BEFORE INPUT CHECK");
    int input_check = check_inputs(&input_udt_balance, &input_wallet_capacity, &input_wallet_count, token_type);
    if (input_check != CKB_SUCCESS) {
      return input_check;
    }
    if (input_wallet_count < 1) {
      return ERROR_NO_INPUT_WALLET_FOUND;
    }
    ckb_debug("AFTER INPUT CHECK");
    // loop through outputs
    // if wallet_count > 1 return error
    // if cell with this script's lock hash and token_type == type_hash is found
    // record udt_out_amt and increment wallet count
    // record capacity amount
    ckb_debug("BEFORE OUPUT CHECK");
    int output_check = check_outputs(&output_udt_balance, &output_wallet_capacity, &output_wallet_count, token_type, lock_hash);
    if (output_check != CKB_SUCCESS) {
      return output_check;
    }
    if (output_wallet_count < 1) {
      return ERROR_NO_OUTPUT_WALLET_FOUND;
    }
    ckb_debug("AFTER OUTPUT CHECK");
    // Verify that diff(in_ckbytes, out_ckbytes) >= ckb_pay_amt
    // Verify that diff(in_udt, out_udt) >= expected_udt_pay
    // Verify that output_wallet_count == input_wallet_count == 1

    int capacity_correct = (output_wallet_capacity == input_wallet_capacity) ||
                           ((output_wallet_capacity > input_wallet_capacity) &&
                            (output_wallet_capacity - input_wallet_capacity) >=
                            ckb_pay_amt);
    int udt_amt_correct = (output_udt_balance > input_udt_balance) &&
                          ((output_udt_balance - input_udt_balance) >=
                          udt_pay_amt);

    int wallet_count_correct = (output_wallet_count == 1 && input_wallet_count == 1);

    if (output_wallet_capacity == input_wallet_capacity) {
      ckb_debug("WALLET CAPACITY IS EQUIVALENT");
    }

    if ((output_wallet_capacity > input_wallet_capacity)) {
      ckb_debug("OUTPUT WALLET CAPACITY > INPUT WALLET CAPACITY");
    }

    if ((output_wallet_capacity - input_wallet_capacity) >=
    ckb_pay_amt) {
      ckb_debug("CAPACITY IS DIFFERENT BUT RATE IS ACHIEVED");
    }

    if (output_udt_balance > input_udt_balance) {
      ckb_debug("OUTPUT TOKEN BALANCE > INPUT TOKEN BALANCE");
    }

    if (output_udt_balance < input_udt_balance) {
      ckb_debug("INPUT TOKEN BALANCE > OUTPUT TOKEN BALANCE");
    }

    if ((output_udt_balance - input_udt_balance) >=
    udt_pay_amt) {
      ckb_debug("OUTPUT UDT BALANCE SATISFIES RATE");
    }
    ckb_debug("MADE IT TO THE END OF WALLET LOCK");
    if (has_sig && verify_secp256k1_blake160_sighash_all(pubkey_hash) == CKB_SUCCESS) {
      return CKB_SUCCESS;
    } else {
      if (capacity_correct && udt_amt_correct && wallet_count_correct) {
        return CKB_SUCCESS;
      } else {
        return ERROR_WALLET_UNLOCK;
      }
    }
}



/**
  Description

  conditions (possible states the script can be in):
    has_sig -- Denotes if the input locked by this script has a signature attached to it
    has_unique_script -- Denotes if the wallet has a single script associated with this wallet

  attributes (elements of the script's state):
    token_type: Denotes the type hash of the token that this wallet stores. The reason for this is that
                without it, the lock script could not enforce payment of a specific token
    udt_rate:   Denotes the amount of token that must be transferred to this wallet in a transaction
    reuse_script_hash: Denotes the script that this wallet is associated with IF the developer wants to
                        only allow funds from the use of a specific script to be stored in this wallet
    pubkey_hash: Denotes the pubkey_hash of the owner of the wallet who is allowed to withdraw the funds


    constraints:
      Funds Amount: The amount transferred to this wallet is equal to:
                    (# of scripts associated w/ this wallet) * udt_rate
      Minimum_positive_deposit: If the wallet is not being unlocked by owner,
                                then output w/ same lock hash must have capacity
                                 == prev_capacity + ckb_rate AND udt_amount ==
                                 prev_udt_amount + Fund Amount
      LockHash_TypeHash_Coupling: The type hash of the wallet cell in inputs & outputs
                                  == token_type arg
      Exact_LockType_Combinations: For each wallet cell in input, there must be exactly one wallet cell
                                  in output with the same lock hash
      Max_Input_Wallets_of_Type: For any cell in inputs, if that cell is a wallet cell,
                                 then no other input cell has the same lock hash

      Uses:
        1. Many to one: Many scripts' usage fees deposited into same wallet
        2. One to one: A single script's usage fees deposited into same wallet

**/
