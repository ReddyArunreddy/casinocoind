//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

//==============================================================================
/*
    2018-05-10  ajochems        CRN Creation
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/crypto/KeyType.h>
#include <casinocoin/net/RPCErr.h>
#include <casinocoin/protocol/ErrorCodes.h>
#include <casinocoin/protocol/JsonFields.h>
#include <casinocoin/protocol/Seed.h>
#include <casinocoin/protocol/PublicKey.h>
#include <casinocoin/rpc/Context.h>

namespace casinocoin {

// {
//   domain_name: <string>   // CRN Domain Name
// }
//
// This command requires Role::ADMIN access because it makes
// no sense to ask an untrusted server for this.
Json::Value doCRNCreate (RPC::Context& context)
{
    if (! context.params.isMember (jss::crn_domain_name))
        return RPC::missing_field_error (jss::crn_domain_name);

    Json::Value obj (Json::objectValue);
    KeyType keyType = KeyType::secp256k1;

    auto seed = randomSeed();

    auto const publicKey = generateKeyPair (keyType, seed).first;
    auto const secretKey = generateKeyPair (keyType, seed).second;

    obj[jss::crn_seed] = toBase58 (seed);
    obj[jss::crn_key] = seedAs1751 (seed);
    obj[jss::crn_account_id] = toBase58(calcAccountID(publicKey));
    obj[jss::crn_public_key] = toBase58(TOKEN_NODE_PUBLIC, publicKey);
    obj[jss::crn_private_key] = toBase58(TOKEN_NODE_PRIVATE, secretKey);
    obj[jss::crn_key_type] = to_string (keyType);
    obj[jss::crn_public_key_hex] = strHex (publicKey.data(), publicKey.size());
    obj[jss::crn_domain_name] = context.params[jss::crn_domain_name].asString();
    obj[jss::crn_domain_signature] = strHex(casinocoin::sign (publicKey, secretKey, makeSlice (context.params[jss::crn_domain_name].asString())));
    return obj;
}

Json::Value doCRNVerify (RPC::Context& context)
{
    if (! context.params.isMember (jss::crn_domain_name))
        return RPC::missing_field_error (jss::crn_domain_name);
    if (! context.params.isMember (jss::crn_domain_signature))
        return RPC::missing_field_error (jss::crn_domain_signature);
    if (! context.params.isMember (jss::crn_public_key))
        return RPC::missing_field_error (jss::crn_public_key);

    Json::Value obj (Json::objectValue);

    boost::optional<PublicKey> publicKey = parseBase58<PublicKey>(TokenType::TOKEN_NODE_PUBLIC, context.params[jss::crn_public_key_hex].asString());
    // const casinocoin::PublicKey& pk = publicKey;
    //casinocoin::verify(pk, makeSlice (context.params[jss::crn_domain_name].asString()), makeSlice (context.params[jss::crn_domain_signature].asString()));
    // obj[jss::crn_public_key] = toBase58(TokenType::TOKEN_NODE_PUBLIC, publicKey);
    obj[jss::crn_valid] = true;
    return obj;
}

} // casinocoin
