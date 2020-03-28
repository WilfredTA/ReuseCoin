#include "ckb_syscalls.h"
#include "common.h"


#include "blockchain.h"
#include "reuse_coin_payment_script.h"

int main() {
  return reuse_coin_verify();
}
