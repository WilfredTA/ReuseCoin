# ReuseCoin

# Motivation

Dapp developers on CKB are able to deploy custom scripts to chain that can be used by others. We have already seen this with some system scripts on CKB such as secp256k1.

This provides an interesting opportunity to dapp developers: the scripts they deploy on chain could benefit many people. Scripts on-chain could be described as open source since anyone can use them and see them.

Unlike traditional open source software, though, this simple model of using any scripts on chain does not implement any form of “software licenses” that describe the restrictions on software use and crediting. There is an opportunity here to make ckb script development into a rich open source activity wherein script developers - as opposed to dapp developers - build scripts for other dapp developers to use. It also provides an opportunity for ckb script developers to monetize their development activities without much effort.

This is what we plan to do: enable ckb script developers to monetize the value they are providing to the ckb development ecosystem.

# Requirements & Scope

## Measuring Value / Impact

If we want to monetize ckb scripts based on their impact, the first thing we must do is measure impact.

Naively, the value of a ckb script could be a simple count of the transactions in which it has been used. However, the limitation of this approach is that ckb scripts’ value have a time component: if two different scripts have been used once, but the first script has been attached to the cell for 1000 blocks and the second script attached to the cell for 10 blocks, then the first script has been used for longer on a piece of **stored** value.

This time component is not always a good useful measure, either, though: some scripts are designed to be used to facilitate a certain type of transaction rather than be attached to a cell at rest.

In other words, some scripts are designed to store assets over the long term, while some scripts are designed to facilitate transactions such that the script is only required to perform the transaction, but not afterward (a good example of this is the model of UDT transfer in which the UDT is temporarily stored in a time locked cell)

Therefore, these two monetization strategies are useful for two different types of scripts.

In summary, there are two ways to measure scripts’ impact:

1. Count of use in tx
2. Measurement of time the script has been attached to cells


In this project, we will build out the infrastructure for the first type.



## Monetization of Value / Impact

The monetization of value/impact depends on the way value / impact is measured. As mentioned above, the first type of impact is what we will aim to complete for the scope of this hackathon.

## Aligning Incentives of Script Devs & Script Users

Script devs want to add value and earn value which translates into: script devs want people to use their scripts and they want to be paid for this.

Script users want it to be easy to use the scripts. Reusing a script already developed by someone else should decrease the amount of work script users (dapp developers) have to do; it shouldn't be cumbersome. Therefore, the additional requirements that construct transactions should not add too much complexity, nor too much size or compute cycles, since both of those will incur costs on the script user, reducing the value-added of the used script.

## Script Discovery

Scripts built by script devs should be discoverable by dapp devs. For this reason, we should provide an online registry in which script devs can submit their scripts and dapp devs can easily find them and see their current usage, as well as documentation on the script.

# System Design

## ReuseCoin Description

ReuseCoin is a protocol for enabling monetization of on-chain scripts. It is not a token itself; rather, it is a mechanism for developers to charge usage fees for their scripts and configure the types of currencies they would like to be compensated with. This means that they can be compensated with a stable coin if they wish, or native CKBytes, or any custom-built token.

All script devs need to do is include reuse coin plugin in their script. This script simply looks inside the dep cells for the reuse coin payment script (which enforces that payment is made) in transactions that use the developer's script, loads the reuse coin payment script and executes the reuse coin payment script to enforce that the payment was made to the developer in the right amount with the right currency.


## Blockchain Components

### ReuseCoin Cell Wallet

Each script developer has a reuse coin cell wallet that is associated with a specific script. When users of a script use the script by the script dev, the transaction generates new tokens and deposits them into the wallet. The wallet is configurable to accept CKBytes or different UDT-standard compliant tokens. Essentially, the reuse coin system can interoperate with all tokens and enables developers to specify how they are paid and the amount they are paid.

Scripts needed:

* Reuse Coin wallet configuration script
* Reuse Coin anyone can pay lock

### Reusable Script Plugin


The reusable script plugin is included in reusable scripts to add the monetization capabilities of the script to the reuse coin.

Scripts needed:

* Reuse Coin Plugin Script
* Reuse Coin Payment Script

## Transaction Patterns

1. Use Script
    1. Mint Reuse Coin (modified mint: causes a transfer to script owner)
2. Create reusable script
3. Exchange for CKBytes

## Script Registry Components

## Script Listing

The script listing is the home page of the script registry app and it lists all reuse coin scripts. It will also regularly update the coins accumulated by each script. A script’s details can be viewed which includes its reference information so that it can be easily included in a transaction by users.

## Submit Script Form

A script dev can submit their script here. The app will verify that the script is reuse coin compatible. Script developers can also provide documentation.

## Include Script Button *

The include script button will copy to the user’s keyboard a skeleton of a transaction that includes the necessary components in order to use the script.


*Features marked with an asterisk are “stretch goals”*
