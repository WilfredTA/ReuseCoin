
// Checks queries for reuse coin on-chain
// If exists, store it, generate reuse coin wallet, send back reuse coin wallet
// If not, return error
// If already stored in DB, return error
const submitReuseCoinScript = async (name, description, authorName, scriptOutPoint, walletOutPoint) => {

}

// returns output of all reuse coin scripts.
// Reuse coin scripts can be identified by:
// Querying chain for cells with reuse coin wallet hash
// Grab the arg in the reuse coin wallet hash, which is the type hash of the script
// Find cells whose type hash is the script
// Sort by amount stored in reuse coin wallet

// return JSON like this:
/**
{
  response: [
    {
      scriptData: {
        previousOutput: {
          txHash: string
          index: string
        }
        depType: code or depGroup
      }
      name: string
      description: string
      authorName: string
      paymentData: {
        tokenTypeHash: string
        usageFee: integer
        walletLockHash: string
      }
    }
  ]
}
**/
const getAllReuseCoinScripts = async () => {

}


// Can get a reuse script by:
// Its associated wallet,
// Its type hash
// Its data hash
const getReuseScriptBy = async (filter={}) => {

}

// Generates tx skeleton in JSON with information to use the script in tx.
const generateReuseScriptTxStructure = async () => {

}
