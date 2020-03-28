#include "blockchain.h"

#include "ckb_syscalls.h"


#define INPUT_OUTPOINT_SIZE 36
#define SCRIPT_SIZE 32768
#define OUTPOINT_INDEX_SIZE 4
#define OUTPOINT_TX_HASH_SIZE 32
#define UDT_AMOUNT_SIZE 8

#define ERROR_ARGUMENTS_LEN -1
#define ERROR_ENCODING -2
#define ERROR_SYSCALL -3
#define ERROR_SCRIPT_TOO_LONG -21
#define ERROR_AMOUNT -52
#define ERROR_OUTPOINT_LOAD -53
#define ERROR_OUTPOINT_ENCODING -54
#define ERROR_OUTPOINT_IDX_SIZE -55
#define ERROR_OUTPOINT_HASH_SIZE -56
#define SCRIPT_ARG_LENGTH (OUTPOINT_TX_HASH_SIZE + OUTPOINT_INDEX_SIZE)
#define INFO_TYPE_ARG_LENGTH (SCRIPT_ARG_LENGTH + 1)

// This function will verify that the first 8 bytes of udt data are occupied by an unsigned integer
int verify_udt_data(){

  uint8_t i = 0;
  while (1) {
    unsigned char data[UDT_AMOUNT_SIZE];
    uint64_t data_size = UDT_AMOUNT_SIZE;
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

    if (MolReader_UdtAmount_verify(&data_seg, data_size) != MOL_OK){
      return ERROR_AMOUNT;
    }
    i += 1;
  }
  i = 0;
  while (1) {
    unsigned char data[UDT_AMOUNT_SIZE];
    uint64_t data_size = UDT_AMOUNT_SIZE;
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

    if (MolReader_UdtAmount_verify(&data_seg, data_size) != MOL_OK){
      return ERROR_AMOUNT;
    }
    i += 1;
  }
  return CKB_SUCCESS;
}

int main() {
  ckb_debug("UDT DEF SCRIPT EXECUTING________");
  // Load in type script args
  int udt_data_is_valid = verify_udt_data();
  if (udt_data_is_valid != CKB_SUCCESS) {
    ckb_debug("UDT DATA FIELD IS INVALID");
    return udt_data_is_valid;
  }

  if (udt_data_is_valid != CKB_SUCCESS) {
    return udt_data_is_valid;
  }
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

  if (args_bytes_seg.size != SCRIPT_ARG_LENGTH) {
    ckb_debug("INCORRECT ARG LENGTH");
    return ERROR_ARGUMENTS_LEN;
  }



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
      ckb_debug("CREATE MODE");
  }

  int info_cell_in_output = 0;
  // If not creation
    // Check if info cell is in output
      // If it is, return success
        // else
      // perform sum verification
  // else if creation
    // Check if info cell in output
    // return success


  if (create_mode == 0) {
    // Locate UDT Info cell
    uint8_t i = 0;
    while (1) {
          ckb_debug("Info type check iteration");
      uint64_t output_type_size = SCRIPT_SIZE;
      unsigned char output_type[SCRIPT_SIZE];
      int load_out_res = ckb_checked_load_cell_by_field(output_type, &output_type_size, 0, i, CKB_SOURCE_OUTPUT, CKB_CELL_FIELD_TYPE);

      if (load_out_res == CKB_INDEX_OUT_OF_BOUND) {
        break;
      }
      if (load_out_res != CKB_SUCCESS) {
        ckb_debug("ERROR loading outputs during info cell check");
        return load_out_res;
      }

      mol_seg_t info_type_seg;
      info_type_seg.ptr = (uint8_t*)output_type;
      info_type_seg.size = output_type_size;

      if (MolReader_Script_verify(&info_type_seg, false) != MOL_OK) {
        ckb_debug("ERROR ENCODING IN INFO CELL TYPE SCRIPT");
        return ERROR_ENCODING;
      }

      mol_seg_t info_args_seg = MolReader_Script_get_args(&info_type_seg);
      mol_seg_t info_args_bytes_seg = MolReader_Bytes_raw_bytes(&info_args_seg);
      if (info_args_bytes_seg.size == INFO_TYPE_ARG_LENGTH) {
        ckb_debug("FOUND CELL W/ ARGS AT INFO TYPE ARG LENGTH");
        uint8_t is_info_cell = info_args_bytes_seg.ptr[OUTPOINT_TX_HASH_SIZE + OUTPOINT_INDEX_SIZE];
        if (is_info_cell == 1) {
          ckb_debug("INFO CELL's LAST ARG IS 1");
          if (memcmp(args_bytes_seg.ptr, info_args_bytes_seg.ptr, OUTPOINT_TX_HASH_SIZE) == 0 &&
                memcmp(&args_bytes_seg.ptr[OUTPOINT_TX_HASH_SIZE],
                  &info_args_bytes_seg.ptr[OUTPOINT_TX_HASH_SIZE], OUTPOINT_INDEX_SIZE) == 0) {
                    ckb_debug("INFO CELL ID MATCHES UUID");
              info_cell_in_output += 1;
              break;
          }
        }
      }
      i += 1;
    }

    ckb_debug("FINISHED INFO CELL CHECK");

    // If info cell not located, perform sum verification
    //
    if (info_cell_in_output == 0) {
      uint64_t total_input = 0;
      uint64_t total_output = 0;
      uint8_t i = 0;
      while (1) {
        uint64_t current = 0;
        uint64_t input_data_len = UDT_AMOUNT_SIZE;
        int input_ret = ckb_checked_load_cell_data((uint8_t*)&current,
          &input_data_len, 0, i, CKB_SOURCE_GROUP_INPUT);
        if (input_ret == CKB_INDEX_OUT_OF_BOUND) {
          break;
        }
        if (input_ret != CKB_SUCCESS) {
          ckb_debug("Error loading inputs during sum verification");
          return input_ret;
        }
        total_input += current;
        i += 1;
      }

      i = 0;
      while (1) {
        uint64_t current = 0;
        uint64_t output_data_len = UDT_AMOUNT_SIZE;
        int output_ret = ckb_checked_load_cell_data((uint8_t*)&current,
          &output_data_len, 0, i, CKB_SOURCE_GROUP_OUTPUT);

        if (output_ret == CKB_INDEX_OUT_OF_BOUND) {
          break;
        }

        if (output_ret != CKB_SUCCESS) {
          ckb_debug("Error loading outputs during sum verification");
          return output_ret;
        }
        total_output += current;
        i += 1;
      }

      if (total_input != total_output) {
        ckb_debug("SUM VERIFICATION FAILED");
        return ERROR_AMOUNT;
      } else {
        ckb_debug("SUM VERIFICATION SUCCESS");
        return CKB_SUCCESS;
      }
    } else {
      return CKB_SUCCESS;
    }


  } else {
    return CKB_SUCCESS;
  }

}
