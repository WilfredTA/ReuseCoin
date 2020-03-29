import config from './boilerplate.js'
import fs from 'fs'




const issueUdt = async (ckb, udtDefInfo) => {

  // -------------------------------- 1. BUILD YOUR OUTPUTS --------------------------
  // Load in Schema
  let { UDTData, SerializeUDTData } = ckb.molecules

  // Create UDT Data
  let udtAmount = ckb.core.utils.hexToBytes(`0x00000000000000000000000000${Number(250000).toString(16)}`).buffer
  let udtDataObj = {
    amount: udtAmount
  }

  // Serialize UDT Data
  //let serializedUdtData = SerializeUDTData(udtDataObj)

  // Verify UDT Data is schema compliant
  //let udtData = new UDTData(serializedUdtData)


  // Create Output
  let udtInstanceOutput = {
    capacity: `0x${ckb.capacityToShannons(10)}`,
    lock: ckb.lockScript,
    type: {
      hashType: "data",
      codeHash: udtDefInfo.dataHash,
      args: ckb.core.utils.serializeArray(udtDefInfo.govHash)
    },
    data: `0x000000000000000000000000000${Number(250000).toString(16)}`
  }

  console.log(udtInstanceOutput)


  //  -------------------------------- 2. GATHER INPUTS & PACKAGE INTO TX --------------------------

  let udtInstanceTx = await ckb.generateTransaction([udtInstanceOutput], {deps: [udtDefInfo.defAsDep]})

  //  -------------------------------- 3. SIGN TRANSACTION --------------------------
  let signedTx = ckb.signTx(udtInstanceTx)


//  -------------------------------- 4. SEND TRANSACTION --------------------------
  let txHash = await ckb.core.rpc.sendTransaction(signedTx)

  return {
    txHash,
    udtInstanceAsInput: ckb.convertToInputCell(txHash),
    udtInstanceTypeHash: ckb.core.utils.scriptToHash(udtInstanceOutput.type),
    defAsDep: udtDefInfo.defAsDep,
    govHash: udtDefInfo.govHash,
    dataHash: udtDefInfo.dataHash,
    udtInstanceType: udtInstanceOutput.type
  }
}


const deployCodeCells = async (ckb) => {
  const udtDefCode = fs.readFileSync('../../verifier/ckb-scripts/build/sudt')

  let udtDefOutput = {
    capacity: `0x${Number(ckb.getCapacityForFile(udtDefCode)).toString(16)}`,
    lock: ckb.lockScript,
    data: ckb.core.utils.bytesToHex(udtDefCode)
  }

  let udtDefTx = await ckb.generateTransaction([udtDefOutput])

  let signedTransaction = ckb.signTx(udtDefTx)

  let txHash = await ckb.core.rpc.sendTransaction(signedTransaction)

  console.log(txHash, "<< UDT Def tx hash")
  return {
    txHash: txHash,
    dataHash: ckb.getHash(ckb.core.utils.bytesToHex(udtDefCode)),
    govHash: ckb.lockHash,
    defAsDep: ckb.convertToDepCell(txHash)
  }
}

const issueUdtWrongGovHash = async (ckb, udtDefInfo) => {
  udtDefInfo.govHash = ckb.lockHash2
  try {
    await issueUdt(ckb, udtDefInfo)
  } catch(e) {
    console.log(e)
    console.log("Should return error -52")
  }
}

const deployTypeIdCode = async (ckb) => {
  const typeIdCode = fs.readFileSync('../../verifier/ckb-scripts/build/type_id')

  let typeIdOutput = {
    capacity: `0x${Number(ckb.getCapacityForFile(typeIdCode)).toString(16)}`,
    lock: ckb.lockScript,
    data: ckb.core.utils.bytesToHex(typeIdCode)
  }

  let typeIdTx = await ckb.generateTransaction([typeIdOutput])

  let signedTransaction = ckb.signTx(typeIdTx)

  let txHash = await ckb.core.rpc.sendTransaction(signedTransaction)

  console.log(txHash, "<< TypeID tx hash")
  return {
    txHash: txHash,
    dataHash: ckb.getHash(ckb.core.utils.bytesToHex(typeIdCode)),
    govHash: ckb.lockHash,
    typeIdAsDep: ckb.convertToDepCell(txHash)
  }
}

const createCellWithTypeId = async (ckb, typeIdInfo) => {
  let { typeId, SerializetypeId} = ckb.molecules

  let cellWithTypeId = {
    capacity: `0x${ckb.capacityToShannons(10)}`,
    lock: ckb.lockScript,
    type: {
      hashType: "data",
      codeHash: typeIdInfo.dataHash,
      args: "0x"
    },
    data: "0x"
  }

  let typeIdTx = await ckb.generateTransaction([cellWithTypeId], {deps:[typeIdInfo.typeIdAsDep]})

  let input1tx = ckb.core.utils.hexToBytes(typeIdTx.inputs[0].previousOutput.txHash).buffer
  let input1idx = ckb.core.utils.hexToBytes(`0x0000${typeIdTx.inputs[0].previousOutput.index}`).buffer
  let typeIdObj = {
    tx_hash: input1tx,
    idx: input1idx
  }
  let serializedTypeId = SerializetypeId(typeIdObj)
  let typeID = new typeId(serializedTypeId)

  typeIdTx.outputs[0].type.args = ckb.core.utils.bytesToHex(new Uint8Array(serializedTypeId))

  let signedTx = ckb.signTx(typeIdTx)
  let txHash = await ckb.core.rpc.sendTransaction(signedTx)

  return {
    txHash,
    cellWithTypeIdAsInput: ckb.convertToInputCell(txHash),
    cellWithTypeIdTypeHash: ckb.core.utils.scriptToHash(cellWithTypeId.type),
    typeIdAsArg: typeIdTx.outputs[0].type.args,
    typeIdAsDep: typeIdInfo.typeIdAsDep,
    dataHash: typeIdInfo.dataHash
  }

}


const updateCellWithTypeId = async (ckb, typeIdInfo) => {
  let { typeId, SerializetypeId} = ckb.molecules

  let cellWithTypeId = {
    capacity: `0x${ckb.capacityToShannons(10)}`,
    lock: ckb.lockScript,
    type: {
      hashType: "data",
      codeHash: typeIdInfo.dataHash,
      args: "0x"
    },
    data: "0x01"
  }

  let typeIdTx = await ckb.generateTransaction([cellWithTypeId], {deps: [typeIdInfo.typeIdAsDep], inputs: [typeIdInfo.cellWithTypeIdAsInput]})
  typeIdTx.outputs[0].type.args = typeIdInfo.typeIdAsArg
 console.log(typeIdTx.outputs[0].type, "<< UPDATE CELL TX")



  let signedTx = ckb.signTx(typeIdTx)
  let txHash = await ckb.core.rpc.sendTransaction(signedTx)

  return {
    txHash,
    cellWithTypeIdAsInput: ckb.convertToInputCell(txHash),
    cellWithTypeIdTypeHash: ckb.core.utils.scriptToHash(cellWithTypeId.type),
    typeIdAsArg: typeIdTx.outputs[0].type.args
  }

}


const transfer = async (ckb, udtDefInfo, amount, recipient) => {
  let { UDTData, SerializeUDTData } = ckb.molecules

  // Create UDT Data
  let udtAmount = ckb.core.utils.hexToBytes(`0x0000000000000000000000000000${Number(amount).toString(16)}`).buffer
  let udtDataObj = {
    amount: udtAmount
  }

  // Serialize UDT Data
  let serializedUdtData = SerializeUDTData(udtDataObj)

  // Verify UDT Data is schema compliant
  let udtData = new UDTData(serializedUdtData)


  // Create Output
  let udtInstanceOutput = {
    capacity: `0x${ckb.capacityToShannons(udtData.view.byteLength)}`,
    lock: recipient,
    type: {
      hashType: "data",
      codeHash: udtDefInfo.dataHash,
      args: ckb.core.utils.serializeArray(udtDefInfo.govHash)
    },
    data: `${ckb.core.utils.bytesToHex(new Uint8Array(serializedUdtData))}`
  }

  console.log(udtInstanceOutput)


  //  -------------------------------- 2. GATHER INPUTS & PACKAGE INTO TX --------------------------

  let udtInstanceTx = await ckb.generateTransaction([udtInstanceOutput], { deps: [udtDefInfo.defAsDep], inputs: [udtDefInfo.udtInstanceAsInput]})

  //  -------------------------------- 3. SIGN TRANSACTION --------------------------
  let signedTx = ckb.signTx(udtInstanceTx)


//  -------------------------------- 4. SEND TRANSACTION --------------------------
  let txHash = await ckb.core.rpc.sendTransaction(signedTx)

  return {
    txHash,
    udtInstanceAsInput: ckb.convertToInputCell(txHash),
    udtInstanceTypeHash: ckb.core.utils.scriptToHash(udtInstanceOutput.type)
  }
}




// -----------------Reuse Coin protocol generator methods --------------

// Prior state: needs a UDT Instance deployed
// UDT Instance object needs following data:
// Deploys the reuse wallet lock script and then deploys a UDT wallet locked with wallet lock
const deployReuseCoinWallet = async (ckb, udtInstance, rate) => {
  // 1. -------------------- Deploy lock code cell -------------
  let code = fs.readFileSync('../../verifier/ckb-scripts/build/reuse_coin_wallet')
  const { pubkeyHash } = ckb
  const {udtInstanceAsInput, udtInstanceTypeHash, udtInstanceType, defAsDep} = udtInstance
  let walletLockScriptOutput = {
    capacity: `0x${Number(ckb.getCapacityForFile(code)).toString(16)}`,
    lock: ckb.lockScript,
    data: ckb.core.utils.bytesToHex(code)
  }

  let walletLockScriptTx = await ckb.generateTransaction([walletLockScriptOutput])

  let signedTransaction = ckb.signTx(walletLockScriptTx)

  let txHash = await ckb.core.rpc.sendTransaction(signedTransaction)

  let walletLockDep = ckb.convertToDepCell(txHash)
  // 2. ---------------------------- Deploy UDT Cell wallet locked w/ above lock script ------------

  // Generate the right args


  let walletArgs = [
    ckb.publicKeyHash,
    `0x00000000000000${Number(15).toString(16)}`,
    `0x000000000000000000000000000000${Number(rate).toString(16)}`,
    udtInstanceTypeHash

  ]


  let udtWalletOutput = {
    capacity: ckb.capacityToShannons(Number(1000)),
    lock: {
      codeHash: ckb.getHash(ckb.core.utils.bytesToHex(code)),
      hashType: 'data',
      args: ckb.core.utils.serializeStruct(walletArgs)
    },
    data: `0x00000000000000000000000000000${Number(1000).toString(16)}`,
    type: udtInstanceType
  }

  let reuseCoinWalletLock = udtWalletOutput.lock
  let udtChangeOutput = {
    capacity: ckb.capacityToShannons(Number(1000)),
    lock: ckb.lockScript2,
    data: `0x000000000000000000000000000${Number(249000).toString(16)}`,
    type: udtInstanceType
  }

  let udtWalletTx = await ckb.generateTransaction([udtWalletOutput,udtChangeOutput], { deps: [udtInstance.defAsDep, walletLockDep], inputs: [udtInstanceAsInput]})

  console.log(udtWalletTx, "<< UDT WALLET TX")
  //  -------------------------------- 3. SIGN TRANSACTION --------------------------
  let signedTx = ckb.signTx(udtWalletTx)


//  -------------------------------- 4. SEND TRANSACTION --------------------------
  let txHash2 = await ckb.core.rpc.sendTransaction(signedTx)

  console.log("UDT WALLET SUCCESSFULLY DEPLOYED")

  return {
    walletLockDep,
    walletCellAsInput: ckb.convertToInputCell(txHash2),
    udtCellAsInput: ckb.convertToInputCell(txHash2, "0x1"),
    reuseCoinWalletLock,
    walletAsOutput: udtWalletOutput,
    udtTypeScriptAsDep: defAsDep,
    udtInstanceType

  }


}


// Deploys a script that includes the reuse coin plugin
// Needs udt info and coin wallet info
// Coin wallet info includes:
// - The wallet lock as a dependency
// - The udt wallet cell as an input
// Udt info includes:
// - Udt definition script as a dependency
const deployReusableScript = async (ckb, udtInfo, coinWalletInfo) => {
  let code = fs.readFileSync('../../verifier/ckb-scripts/build/example_reuse')


  const { walletLockDep, walletCellAsInput, udtCellAsInput, reuseCoinWalletLock, udtTypeScriptAsDep, walletAsOutput } = coinWalletInfo
  const { govHash, udtInstanceTypeHash, udtInstanceType } = udtInfo
  let reusableScriptOutput = {
    capacity: ckb.capacityToShannons(1900),
    lock: reuseCoinWalletLock,
    data: ckb.core.utils.bytesToHex(code)
  }

  const reusableScriptDataHash = ckb.getHash(reusableScriptOutput.data)

  let reusableScriptTx = await ckb.generateTransaction([reusableScriptOutput], {deps: [walletLockDep]})

  console.log(reusableScriptTx, "<< REUSABLE SCRIPT DEPLOYMENT TX")
  let signedTransaction = ckb.signTx(reusableScriptTx)

  let txHash = await ckb.core.rpc.sendTransaction(signedTransaction)

  return {
    walletLockDep,
    reusableScriptDep: ckb.convertToDepCell(txHash),
    walletCellAsInput,
    udtCellAsInput,
    reuseCoinWalletLock,
    reusableScriptDataHash,
    walletAsOutput,
    udtTypeScriptAsDep,
    udtInstanceType

  }

}



// This transaction uses a reusable script and pays the developer
// Needs:
// 1. UDT instance
// 2. udt type hash
// 3. the reuseable coin wallet cell of the script owner
// 4. the reusable coin wallet lock dep cell
// 5.
const useReusableScript = async (ckb, reuseCoinWalletInfo) => {
  const { walletLockDep, reusableScriptDep, walletCellAsInput, udtCellAsInput,
    reuseCoinWalletLock, reusableScriptDataHash, walletAsOutput, udtTypeScriptAsDep, udtInstanceType } = reuseCoinWalletInfo

  const { ReuseCoinArgs, SerializeReuseCoinArgs, SerializeUDTData, UDTData } = ckb.molecules

  let reuseCoinWalletLockHash = ckb.core.utils.scriptToHash(reuseCoinWalletLock)
  let walletHashAsArg = reuseCoinWalletLockHash


  // Create output 1: Output using the reusable script
  let useOutput = {
    capacity: ckb.capacityToShannons(500),
    data: "0x",
    lock: ckb.lockScript2,
    type: {
      codeHash: reusableScriptDataHash,
      hashType: "data",
      args: walletHashAsArg
    }
  }

  // Create output 2: The reuse coin wallet owned by script developer

  let newUdtAmount = `0x0000000000000000000000000000${Number(10000).toString(16)}`

  let walletOutput = {
    capacity: ckb.capacityToShannons(1000),
    type: udtInstanceType,
    lock: reuseCoinWalletLock,
    data: newUdtAmount
  }

  // Create output 3: the leftover udt after paying usage fee 

  let udtChangeAmt = `0x00000000000000000000000000${Number(248000).toString(16)}`


  let udtChangeOutput = {
    capacity: ckb.capacityToShannons(Number(1000)),
    lock: ckb.lockScript2,
    data: udtChangeAmt,
    type: udtInstanceType
  }

  let additional = {
    deps: [walletLockDep, reusableScriptDep, udtTypeScriptAsDep],
    inputs: [walletCellAsInput, udtCellAsInput]
  }
  let useReusableScriptTx = await ckb.generateTransaction([useOutput, walletOutput], additional, ckb.lockHash2)


  console.log(useReusableScriptTx, "<< USE SCRIPT TX")
  let signedTx = ckb.signTx2(useReusableScriptTx)

  console.log(signedTx, "<< SIGNED TX")
  let txhash = await ckb.core.rpc.sendTransaction(signedTx)
}

const run = async () => {
  try {
    let ckb = await config();
    //await runTypeId(ckb)
    //await runUDTDeployment(ckb)
    await runReuseCoinEndToEnd(ckb)
  } catch(e) {
    throw e
  }
}

const runReuseCoinEndToEnd = async (ckb) => {
  try {
    let udtDefInfo = await deployCodeCells(ckb)
    console.log("UDT DEF DEPLOYED")
    let udtInstance = await issueUdt(ckb, udtDefInfo)
    console.log("UDT INSTANCE DEPLOYED")
    let coinWallet = await deployReuseCoinWallet(ckb, udtInstance, 100)
    let reusableScriptInfo = await deployReusableScript(ckb, udtInstance, coinWallet)
    let scriptUse = await useReusableScript(ckb, reusableScriptInfo)
  } catch (e) {
    throw e
  }
}

const runTypeId = async (ckb) => {
  try {
    let typeIdCode = await deployTypeIdCode(ckb)
    let cellWithTypeId = await createCellWithTypeId(ckb, typeIdCode)
    let updatedCellWithTypeId = await updateCellWithTypeId(ckb, cellWithTypeId)
  } catch(e) {
    throw e
  }
}

const runUDTDeployment = async (ckb) => {
  try {
    let udtDefInfo = await deployCodeCells(ckb)
    let udtInstance = await issueUdt(ckb, udtDefInfo)
    //let transferRes = await transfer(ckb, udtInstance, 1000, ckb.lockScript2)
  } catch(e) {
    throw e
  }
}

run()
