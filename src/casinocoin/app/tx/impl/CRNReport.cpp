//------------------------------------------------------------------------------
/*
    This file is part of casinocoind: https://github.com/casinocoin/casinocoind

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
    2018-01-11  jrojek          created
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/app/tx/impl/CRNReport.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/core/Config.h>
#include <casinocoin/protocol/Feature.h>
#include <casinocoin/protocol/Indexes.h>
#include <casinocoin/protocol/PublicKey.h>
#include <casinocoin/protocol/Quality.h>
#include <casinocoin/protocol/st.h>
#include <casinocoin/ledger/View.h>
namespace casinocoin {

uint64_t
CRNReport::calculateBaseFee(const PreclaimContext &ctx)
{
    return 0;
}

TER
CRNReport::preflight (PreflightContext const& ctx)
{
    auto const ret = preflight1 (ctx);
    if (!isTesSuccess (ret))
        return ret;

    auto& tx = ctx.tx;
    auto& j = ctx.j;

    // CRNReport transaction is enabled via amendment
    if (!ctx.rules.enabled(featureCRN))
    {
        JLOG(j.info()) << "CRNReport: Feature CRN disabled.";
        return temDISABLED;
    }

    std::uint32_t const uSetFlag = tx.getFieldU32 (sfSetFlag);
    std::uint32_t const uClearFlag = tx.getFieldU32 (sfClearFlag);

    if ((uSetFlag != 0) && (uSetFlag == uClearFlag))
    {
        JLOG(j.info()) << "CRNReport: Malformed transaction: Set and clear same flag.";
        return temINVALID_FLAG;
    }

    auto const id = tx.getAccountID(sfAccount);
    if (id == zero)
        return temBAD_SRC_ACCOUNT;

    // jrojek TODO: verify that account is on CRN list

    // No point in going any further if the transaction fee is malformed.
    auto const fee = tx.getFieldAmount (sfFee);
    if (!fee.native () || fee != beast::zero)
    {
        JLOG(j.warn()) << "CRNReport: invalid fee";
        return temBAD_FEE;
    }

    return preflight2(ctx);
}

TER
CRNReport::doApply ()
{
    JLOG(j_.info()) << "applyCRN_Report!";
    AccountID const crnAccountID (ctx_.tx.getAccountID (sfAccount));

    // Open a ledger for editing.
    SLE::pointer sleAcc = view().peek (keylet::account(crnAccountID));

    if (!sleAcc)
    {
        JLOG(j_.warn()) << "Sourceccount does not exist. Kind of impossible";
        return terNO_ACCOUNT;
    }
    else
    {
        // mark account to update
        view().update (sleAcc);
    }

    std::uint32_t const uFlagsIn = sleAcc->getFieldU32 (sfFlags);
    std::uint32_t uFlagsOut = uFlagsIn;

    std::uint32_t const uSetFlag = ctx_.tx.getFieldU32 (sfSetFlag);
    std::uint32_t const uClearFlag = ctx_.tx.getFieldU32 (sfClearFlag);

    // jrojek not for now
//    if (!(uFlagsIn & lsfKYCValidated))
//    {
//        JLOG(j_.warn()) << "Account is not KYC validated. Please fill KYC first";
//        return temINVALID;
//    }

    auto crnObject = sleAcc->peekFieldObject(sfCRN);

    // PubKey
    if (crnObject.isFieldPresent(sfCRN_PublicKey))
    {
        if (makeSlice(crnObject.getFieldVL(sfCRN_PublicKey)) != makeSlice(ctx_.tx.getFieldVL(sfSigningPubKey)))
        {
            JLOG(j_.warn()) << "Public Key mismatch. Should actually blacklist this node";
            return temMALFORMED;
        }
    }
    else
        crnObject.setFieldVL(sfCRN_PublicKey, makeSlice(ctx_.tx.getFieldVL(sfSigningPubKey)));

    // IPAddress
    if (crnObject.isFieldPresent(sfCRN_IPAddress))
    {
        if (makeSlice(crnObject.getFieldVL(sfCRN_IPAddress)) != makeSlice(ctx_.tx.getFieldVL(sfCRN_IPAddress)))
        {
            JLOG(j_.warn()) << "IPAddress mismatch. Should actually blacklist this node";
            return temMALFORMED;
        }
    }
    else
        crnObject.setFieldVL(sfCRN_IPAddress, makeSlice(ctx_.tx.getFieldVL(sfCRN_IPAddress)));

    //Domain
    if (crnObject.isFieldPresent(sfCRN_DomainName))
    {
        if (makeSlice(crnObject.getFieldVL(sfCRN_DomainName)) != makeSlice(ctx_.tx.getFieldVL(sfCRN_DomainName)))
        {
            JLOG(j_.warn()) << "Domain mismatch. Should actually blacklist this node";
            return temMALFORMED;
        }
    }
    else
        crnObject.setFieldVL(sfCRN_DomainName, makeSlice(ctx_.tx.getFieldVL(sfCRN_DomainName)));

    //Latency average between flag ledgers
    crnObject.setFieldU32(sfCRN_LatencyAvg, ctx_.tx.getFieldU32(sfCRN_LatencyAvg));
    crnObject.setFieldArray(sfCRN_ConnectionStats, ctx_.tx.getFieldArray(sfCRN_ConnectionStats));

    // jrojek TODO will be evaluated later
//    if (uFlagsIn != uFlagsOut)
//        sleAcc->setFieldU32 (sfFlags, uFlagsOut);


    sleAcc->setFieldObject(sfCRN, crnObject);

    JLOG(j_.warn()) << "CRN Report for account: " << toBase58(account_) << " is applied";
    return tesSUCCESS;
}

}

