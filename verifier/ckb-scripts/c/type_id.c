// This script implements type ID functionality
// It is only a POC. Please use w/ caution

#include "ckb_syscalls.h"
#include "blockchain.h"


#define TX_HASH_SIZE 32
#define TX_IDX_SIZE 4
#define MAX_SCRIPT_SIZE 32768
#define OUTPOINT_SIZE (TX_HASH_SIZE + TX_IDX_SIZE)

#define ERROR_TYPE_ID_VIOLATION -61
#define ERROR_OUTPOINT_IDX_SIZE -62
#define ERROR_OUTPOINT_HASH_SIZE -63

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
        ckb_debug("TOO MANY TYPE ID CELLS IN INPUT");
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
        ckb_debug("TOO MANY TYPE ID CELLS IN OUTPUT");
        return ERROR_TYPE_ID_VIOLATION;
      }
      i += 1;
    }

    if (out_count == 1) {
      ckb_debug("ONE TYPE ID CELL DETECTED IN OUTPUT");
    }

    if (in_count >= 1) {
      ckb_debug("ONE TYPE ID CELL DETECTED IN INPUT");
    }
    if (out_count == 1 && in_count == 1) {
      return CKB_SUCCESS;
    } else {
      return ERROR_TYPE_ID_VIOLATION;
    }
  }

int verify_type_id_creation() {
  unsigned char scriptData[MAX_SCRIPT_SIZE];
  uint64_t len = MAX_SCRIPT_SIZE;

  int ret = ckb_load_script(scriptData, &len, 0);
  if (ret != CKB_SUCCESS) {
    return ret;
  }

  mol_seg_t script;
  script.ptr = scriptData;
  script.size = len;

  mol_seg_t script_args = MolReader_Script_get_args(&script);
  mol_seg_t raw_args = MolReader_Bytes_raw_bytes(&script_args);

  if (MolReader_typeId_verify(&raw_args, false) != MOL_OK) {
    return ERROR_TYPE_ID_VIOLATION;
  }

  mol_seg_t tx_hash = MolReader_typeId_get_tx_hash(&raw_args);
  mol_seg_t tx_idx = MolReader_typeId_get_idx(&raw_args);

  unsigned char input[OUTPOINT_SIZE];
  uint64_t input_len = OUTPOINT_SIZE;
  int in_ret = ckb_checked_load_input_by_field(input, &input_len, 0, 0,
  CKB_SOURCE_INPUT, CKB_INPUT_FIELD_OUT_POINT);
  if (in_ret != CKB_SUCCESS) {
    return in_ret;
  }
  mol_seg_t outpoint_seg;
  outpoint_seg.ptr = (uint8_t*)input;
  outpoint_seg.size = input_len;
  if (MolReader_OutPoint_verify(&outpoint_seg, input_len) != MOL_OK) {
    ckb_debug("Error in outpoint encoding!");
    return ERROR_TYPE_ID_VIOLATION;
  }

mol_seg_t outpoint_tx_hash = MolReader_OutPoint_get_tx_hash(&outpoint_seg);
mol_seg_t outpoint_tx_idx = MolReader_OutPoint_get_index(&outpoint_seg);

if (outpoint_tx_hash.size != TX_HASH_SIZE) {
    ckb_debug("ERROR TX HASH SIZE");
    return ERROR_OUTPOINT_HASH_SIZE;
  }

  if (outpoint_tx_idx.size != TX_IDX_SIZE) {
    ckb_debug("ERROR TX IDX SIZE");
    return ERROR_OUTPOINT_IDX_SIZE;
  }

  int create_mode = 0;
  if (memcmp(tx_hash.ptr, outpoint_tx_hash.ptr, TX_HASH_SIZE) == 0 &&
        memcmp(tx_idx.ptr, outpoint_tx_idx.ptr, TX_IDX_SIZE) == 0) {
      create_mode = 1;
      ckb_debug("CREATE MODE");
  }

  if (create_mode == 1) {
    return CKB_SUCCESS;
  } else {
    return ERROR_TYPE_ID_VIOLATION;
  }

}

int main() {
  int valid_creation = verify_type_id_creation();

  if (valid_creation == CKB_SUCCESS) {
    return CKB_SUCCESS;
  }

  int valid_update = verify_type_id_update();

  if (valid_update == CKB_SUCCESS) {
    return CKB_SUCCESS;
  }

  return ERROR_TYPE_ID_VIOLATION;

}
