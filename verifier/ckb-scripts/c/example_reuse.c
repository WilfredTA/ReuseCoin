#include "ckb_syscalls.h"
#include "common.h"


#include "blockchain.h"
#include "reuse_coin_payment_script.h"

int main() {
  ckb_debug("I'm a reusable script! You have to pay my developer in order to use this!\n");
  return reuse_coin_verify();
}
