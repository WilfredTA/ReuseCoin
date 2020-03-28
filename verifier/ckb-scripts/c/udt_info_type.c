#include "ckb_syscalls.h"
#include "blockchain.h"

#define INPUT_OUTPOINT_SIZE 36
#define SCRIPT_SIZE 32768
#define OUTPOINT_INDEX_SIZE 4
#define OUTPOINT_TX_HASH_SIZE 32
#define UDT_AMOUNT_SIZE 8
#define MAX_DATA_SIZE 32768



#define ERROR_ARGUMENTS_LEN -1
#define ERROR_ENCODING -2
#define ERROR_SYSCALL -3
#define ERROR_SCRIPT_TOO_LONG -21
#define ERROR_OVERFLOWING -51
#define ERROR_AMOUNT -52
#define ERROR_OUTPOINT_LOAD -53
#define ERROR_OUTPOINT_ENCODING -54
#define ERROR_OUTPOINT_IDX_SIZE -55
#define ERROR_OUTPOINT_HASH_SIZE -56
#define ERROR_UNAUTHORIZED_BURN -58

#define ERROR_TYPE_ID_VIOLATION -61
#define ERROR_MINT_VALIDATION -62
#define ERROR_DATA_FIELD -63

#define SCRIPT_ARG_LENGTH (OUTPOINT_TX_HASH_SIZE + OUTPOINT_INDEX_SIZE + 1)
#define UDT_INSTANCE_SCRIPT_ARG_LENGTH (OUTPOINT_TX_HASH_SIZE + OUTPOINT_INDEX_SIZE)

// Args:
// Compound UUID
//  1. Tx hash of first input's outpoint during UDT info cell creation
//  2. Tx idx of first input's outpoint during UDT info cell creation
//  3. is_info_cell flag

// verify_type_id_update Logic
// -----------------------
// Checks script group for exactly one input & output of info cell. This ensures:
// 1. That info cell is not destroyed in a transaction
// 2. That only one info cell with this particular ID can exist
// 3. And therefore allows other scripts, such as those on UDT instances, to easily
//  detect the correct info cell in a transaction (as opposed to a fake one
int verify_type_id_update() {
    uint64_t len = 0;
    uint64_t i = 0;
    uint8_t in_count = 0;
    uint8_t out_count = 0;
    while (1) {
      int ret = ckb_load_input_by_field(NULL, &len, 0, i, CKB_SOURCE_GROUP_INPUT, CKB_INPUT_FIELD_SINCE);
      if (ret == CKB_INDEX_OUT_OF_BOUND) {
        break;
      } else {
        in_count += 1;
      }
      if (in_count > 1) {
        ckb_debug("TOO MANY INFO CELLS IN INPUT");
        return ERROR_TYPE_ID_VIOLATION;
      }
      i += 1;
    }

    i = 0;
    while(1) {
      int ret = ckb_load_cell_by_field(NULL, &len, 0, i, CKB_SOURCE_GROUP_OUTPUT, CKB_CELL_FIELD_CAPACITY);
      if (ret == CKB_INDEX_OUT_OF_BOUND) {
        break;
      } else {
        out_count += 1;
      }
      if (out_count > 1) {
        ckb_debug("TOO MANY INFO CELLS IN OUTPUT");
        return ERROR_TYPE_ID_VIOLATION;
      }
      i += 1;
    }

    if (out_count == 1) {
      ckb_debug("ONE INFO CELL DETECTED IN OUTPUT");
    }

    if (in_count >= 1) {
      ckb_debug("ONE INFO CELL DETECTED IN INPUT");
    }
    if (out_count == 1 && in_count == 1) {
      return CKB_SUCCESS;
    } else {
      return ERROR_TYPE_ID_VIOLATION;
    }
  }


int get_udt_instance_amount(int source, uint8_t* target_id, uint64_t*amt) {
  uint64_t total_amt = 0;
  int i = 0;
  int total_instances_checked = 0;

  while (1) {
    unsigned char udt_script[SCRIPT_SIZE];
    uint64_t script_size = SCRIPT_SIZE;
    int load_udt_ret = ckb_checked_load_cell_by_field(udt_script, &script_size, 0,
      i, source, CKB_CELL_FIELD_TYPE);
    if (load_udt_ret == CKB_INDEX_OUT_OF_BOUND) {
      ckb_debug("INDEX OUT OF BOUND");
      break;
    }
    total_instances_checked += 1;
    if (load_udt_ret != CKB_SUCCESS && load_udt_ret != CKB_ITEM_MISSING) {
      ckb_debug("Error loading udt_instance_amount");
      return load_udt_ret;
    }
    mol_seg_t script_seg;
    script_seg.ptr = (uint8_t*)udt_script;
    script_seg.size = script_size;

    mol_seg_t args_seg = MolReader_Script_get_args(&script_seg);
    mol_seg_t args_bytes_seg = MolReader_Bytes_raw_bytes(&args_seg);

    if (args_bytes_seg.size == UDT_INSTANCE_SCRIPT_ARG_LENGTH) {
      ckb_debug("INSIDE GET INSTANCE, FOUND ONE W? CORRECT ARG LENGTH");
      if (memcmp(args_bytes_seg.ptr, target_id, OUTPOINT_TX_HASH_SIZE) == 0 &&
          memcmp(&args_bytes_seg.ptr[OUTPOINT_TX_HASH_SIZE], &target_id[OUTPOINT_TX_HASH_SIZE], OUTPOINT_INDEX_SIZE) == 0) {
            ckb_debug("FOUND AN INSTANCE");
            uint64_t udt_amount;
            uint64_t amount_size = UDT_AMOUNT_SIZE;
            int load_amount_ret = ckb_checked_load_cell_data((uint8_t*)&udt_amount, &amount_size, 0, i, source);

            if (load_amount_ret != CKB_SUCCESS) {
              ckb_debug("ERROR LOADING INSTANCE AMOUNT IN DATA LOAD CALL");
              return load_amount_ret;
            }
            if (udt_amount == 200) {
              ckb_debug("UDT AMOUNT IS 200 INSIDE LOAD INSTANCE CALL");
            }
            if (udt_amount == 100) {
              ckb_debug("UDT AMOUNT IS 100 INSIDE LOAD INSTANCE CALL");
            }
            total_amt += udt_amount;
          }
    }

    i += 1;
  }
  *amt = total_amt;
  return CKB_SUCCESS;
}

// verify_data Logic (upcoming in next demo)
// -----------------
// Checks type ID's data field to enforce data change rules and enforce a schema. Ensures:
// 1. Data field is a dynamic vector
// 2. First field in vector is 8 byte unsigned integer (uint64_t) total_supply
// 3. Second field in vector is 8 byte unsigned integer (uint64_t) mint_amount_threshold
// 4. Third field in vector is a relative time_stamp mint_interval
// 5. If not creation tx and info cell is in output of current tx, checks the block number from the
//   info cell in input and ensures that it is >= mint_interval blocks in the past
// 6. Enforce immutability of certain fields if necessary
int verify_data() {
  uint8_t i = 0;
  while (1) {
    unsigned char data[MAX_DATA_SIZE];
    uint64_t data_size = MAX_DATA_SIZE;
    int load_data_ret = ckb_checked_load_cell_data(data, &data_size, 0, i, CKB_SOURCE_GROUP_INPUT);
    if (load_data_ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (load_data_ret != CKB_SUCCESS) {
      return load_data_ret;
    }
    mol_seg_t data_seg;
    data_seg.ptr = (uint8_t *)data;
    data_seg.size = data_size;

    if (MolReader_UdtInfo_verify(&data_seg, false) != MOL_OK){
      return ERROR_DATA_FIELD;
    }
    i += 1;
  }
  i = 0;
  while (1) {
    unsigned char data[MAX_DATA_SIZE];
    uint64_t data_size = MAX_DATA_SIZE;
    int load_data_ret = ckb_checked_load_cell_data(data, &data_size, 0, i, CKB_SOURCE_GROUP_OUTPUT);
    if (load_data_ret == CKB_INDEX_OUT_OF_BOUND) {
      break;
    }
    if (load_data_ret != CKB_SUCCESS) {
      return load_data_ret;
    }
    mol_seg_t data_seg;
    data_seg.ptr = (uint8_t *)data;
    data_seg.size = data_size;

    if (MolReader_UdtInfo_verify(&data_seg, false) != MOL_OK){
      return ERROR_DATA_FIELD;
    }
    i += 1;
  }
  return CKB_SUCCESS;
}


// Main verification logic
// This function will:
// 1. check if the current tx is *creating* the info cell
// 2. If it is creation, it will verify the data field and return
// 3. If it is not a create tx, it will verify data field and any changes, check if mint
//  is occurring and verify the mint tx as well
int main() {
  ckb_debug("INFO CELL TYPE SCRIPT EXECUTING_________");
  unsigned char script[SCRIPT_SIZE];
  uint64_t len = SCRIPT_SIZE;

  int script_load_res = ckb_checked_load_script(script, &len, 0);

  if (script_load_res != CKB_SUCCESS) {
    ckb_debug("SYSCALL ERROR");
    return ERROR_SYSCALL;
  }
  if (len > SCRIPT_SIZE) {
    ckb_debug("SCRIPT TOO LONG");
    return ERROR_SCRIPT_TOO_LONG;
  }

  mol_seg_t script_seg;
  script_seg.ptr = (uint8_t*)script;
  script_seg.size = len;

  if (MolReader_Script_verify(&script_seg, false) != MOL_OK) {
    ckb_debug("ERROR ENCODING IN SCRIPT");
    return ERROR_ENCODING;
  }

  mol_seg_t args_seg = MolReader_Script_get_args(&script_seg);
  mol_seg_t args_bytes_seg = MolReader_Bytes_raw_bytes(&args_seg);

  if (args_bytes_seg.size < SCRIPT_ARG_LENGTH) {
    ckb_debug("INFO TYPE SCRIPT arg length smaller than expected");
  } else if (args_bytes_seg.size > SCRIPT_ARG_LENGTH) {
    ckb_debug("INFO TYPE SCRIPT arg length bigger than expected");
  }
  if (args_bytes_seg.size != SCRIPT_ARG_LENGTH) {
    ckb_debug("INCORRECT INFO TYPE sARG LENGTH");
    return ERROR_ARGUMENTS_LEN;
  }

  // Verify data field contains total supply as 8 byte


  // Load in first input

  unsigned char input[INPUT_OUTPOINT_SIZE];
  uint64_t in_len = INPUT_OUTPOINT_SIZE;
  int in_ret = ckb_checked_load_input_by_field(input, &in_len, 0, 0,
    CKB_SOURCE_INPUT, CKB_INPUT_FIELD_OUT_POINT);
  if (in_ret != CKB_SUCCESS) {
    return ERROR_OUTPOINT_LOAD;
  }
  mol_seg_t outpoint_seg;
  outpoint_seg.ptr = (uint8_t*)input;
  outpoint_seg.size = in_len;
  if (MolReader_OutPoint_verify(&outpoint_seg, in_len) != MOL_OK) {
    ckb_debug("Error in outpoint encoding!");
    return ERROR_OUTPOINT_ENCODING;
  }

  mol_seg_t outpoint_tx_hash = MolReader_OutPoint_get_tx_hash(&outpoint_seg);
  mol_seg_t outpoint_tx_idx = MolReader_OutPoint_get_index(&outpoint_seg);

  if (outpoint_tx_hash.size != OUTPOINT_TX_HASH_SIZE) {
    ckb_debug("ERROR TX HASH SIZE");
    return ERROR_OUTPOINT_HASH_SIZE;
  }

  if (outpoint_tx_idx.size != OUTPOINT_INDEX_SIZE) {
    ckb_debug("ERROR TX IDX SIZE");
    return ERROR_OUTPOINT_IDX_SIZE;
  }

  // Check if UDT creation
  int create_mode = 0;
  if (memcmp(args_bytes_seg.ptr, outpoint_tx_hash.ptr, OUTPOINT_TX_HASH_SIZE) == 0 &&
        memcmp(&args_bytes_seg.ptr[OUTPOINT_TX_HASH_SIZE], outpoint_tx_idx.ptr, OUTPOINT_INDEX_SIZE) == 0) {
      create_mode += 1;
      ckb_debug("CREATE MODE IN INFO CELL");
  }

  if (create_mode) {
    return CKB_SUCCESS;
  } else {
    // Ensure that exactly one input & output in script group
    int valid_id_transformation = verify_type_id_update();
    if (valid_id_transformation == CKB_SUCCESS) {
      ckb_debug("VALID TYPE ID TRANSFORMATION ON INFO CELL");
      uint64_t in_supply;
      uint64_t out_supply;
      uint64_t out_supply_size = UDT_AMOUNT_SIZE;
      int out_supply_ret = ckb_checked_load_cell_data((uint8_t*)&out_supply, &out_supply_size, 0, 0, CKB_SOURCE_GROUP_OUTPUT);
      if (out_supply_ret != CKB_SUCCESS) {
        ckb_debug("Error loading total supply from info cell in outputs");
        return out_supply_ret;
      }

      uint64_t in_supply_size = UDT_AMOUNT_SIZE;

      int in_supply_ret = ckb_checked_load_cell_data((uint8_t*)&in_supply, &in_supply_size, 0, 0, CKB_SOURCE_GROUP_INPUT);
      if (in_supply_ret != CKB_SUCCESS) {
        ckb_debug("Error loading total supply from info cell in inputs");
        return in_supply_ret;
      }

      uint64_t total_input_instance_amt;
      uint64_t total_output_instance_amt;
      int load_amt_of_inputs_ret = get_udt_instance_amount(CKB_SOURCE_INPUT, args_bytes_seg.ptr, &total_input_instance_amt);

      if (load_amt_of_inputs_ret != CKB_SUCCESS) {
        ckb_debug("ERROR IN LOAD AMT OF OUTPUTS_RET");
        return load_amt_of_inputs_ret;
      }

      int load_amt_of_outputs_ret = get_udt_instance_amount(CKB_SOURCE_OUTPUT,args_bytes_seg.ptr, &total_output_instance_amt);

      if (load_amt_of_outputs_ret != CKB_SUCCESS) {
        ckb_debug("ERROR IN LOAD AMT OF OUTPUTS_RET");
        return load_amt_of_outputs_ret;
      }

      uint64_t diff = 0;
      if (out_supply > in_supply) {
        diff = out_supply - in_supply;
      }

      if (diff > 0 || total_output_instance_amt > total_input_instance_amt) {
        ckb_debug("OUT SUPPLY GREATER THAN IN SUPPLY ON TRANSACTION");

        if ((total_output_instance_amt > total_input_instance_amt) &&
            ((total_output_instance_amt - total_input_instance_amt) == diff)) {
              ckb_debug("SUCCESSFUL MINT");
              return CKB_SUCCESS;
            } else {
              ckb_debug("MINT FAILED");
              return ERROR_MINT_VALIDATION;
            }
      } else if (in_supply > out_supply) {
        return ERROR_UNAUTHORIZED_BURN;
      } else {
        return CKB_SUCCESS;
      }

    } else {
      ckb_debug("INVALID TYPE ID TRANSFORMATION ON INFO CELL");
      return ERROR_TYPE_ID_VIOLATION;
    }
  }
}
