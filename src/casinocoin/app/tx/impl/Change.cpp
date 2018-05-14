//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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
    2017-06-28  ajochems        Refactored for casinocoin
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/app/tx/impl/Change.h>
#include <casinocoin/app/main/Application.h>
#include <casinocoin/app/misc/AmendmentTable.h>
#include <casinocoin/app/misc/NetworkOPs.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/protocol/Indexes.h>
#include <casinocoin/protocol/TxFlags.h>

namespace casinocoin {

TER
Change::preflight (PreflightContext const& ctx)
{
    auto const ret = preflight0(ctx);
    if (!isTesSuccess(ret))
        return ret;

    auto account = ctx.tx.getAccountID(sfAccount);
    if (account != zero)
    {
        JLOG(ctx.j.warn()) << "Change: Bad source id";
        return temBAD_SRC_ACCOUNT;
    }

    // No point in going any further if the transaction fee is malformed.
    auto const fee = ctx.tx.getFieldAmount (sfFee);
    if (!fee.native () || fee != beast::zero)
    {
        JLOG(ctx.j.warn()) << "Change: invalid fee";
        return temBAD_FEE;
    }

    if (!ctx.tx.getSigningPubKey ().empty () ||
        !ctx.tx.getSignature ().empty () ||
        ctx.tx.isFieldPresent (sfSigners))
    {
        JLOG(ctx.j.warn()) << "Change: Bad signature";
        return temBAD_SIGNATURE;
    }

    if (ctx.tx.getSequence () != 0 || ctx.tx.isFieldPresent (sfPreviousTxnID))
    {
        JLOG(ctx.j.warn()) << "Change: Bad sequence";
        return temBAD_SEQUENCE;
    }

    return tesSUCCESS;
}

TER
Change::preclaim(PreclaimContext const &ctx)
{
    // If tapOPEN_LEDGER is resurrected into ApplyFlags,
    // this block can be moved to preflight.
    if (ctx.view.open())
    {
        JLOG(ctx.j.warn()) << "Change transaction against open ledger";
        return temINVALID;
    }

    if (ctx.tx.getTxnType() != ttAMENDMENT
        && ctx.tx.getTxnType() != ttFEE
        && ctx.tx.getTxnType() != ttCRN_REPORT
        && ctx.tx.getTxnType() != ttCRN_ROUND)
        return temUNKNOWN;

    return tesSUCCESS;
}


TER
Change::doApply()
{
    if (ctx_.tx.getTxnType () == ttAMENDMENT)
        return applyAmendment ();

    if (ctx_.tx.getTxnType () == ttFEE)
        return applyFee();
    if (ctx_.tx.getTxnType () == ttCRN_REPORT)
        return applyCRN_Report ();
    assert(ctx_.tx.getTxnType() == ttCRN_ROUND);
    return applyCRN_Round ();
}

void
Change::preCompute()
{
    account_ = ctx_.tx.getAccountID(sfAccount);

    if (ctx_.tx.getTxnType() == ttCRN_REPORT)
    {
        // jrojek TODO: verify that account is on CRN list
        assert(account_ != zero);
    }
    else
        assert(account_ == zero);
}

TER
Change::applyAmendment()
{
    uint256 amendment (ctx_.tx.getFieldH256 (sfAmendment));

    auto const k = keylet::amendments();

    SLE::pointer amendmentObject =
        view().peek (k);

    if (!amendmentObject)
    {
        amendmentObject = std::make_shared<SLE>(k);
        view().insert(amendmentObject);
    }

    STVector256 amendments =
        amendmentObject->getFieldV256(sfAmendments);

    if (std::find (amendments.begin(), amendments.end(),
            amendment) != amendments.end ())
        return tefALREADY;

    auto flags = ctx_.tx.getFlags ();

    const bool gotMajority = (flags & tfGotMajority) != 0;
    const bool lostMajority = (flags & tfLostMajority) != 0;

    if (gotMajority && lostMajority)
        return temINVALID_FLAG;

    STArray newMajorities (sfMajorities);

    bool found = false;
    if (amendmentObject->isFieldPresent (sfMajorities))
    {
        const STArray &oldMajorities = amendmentObject->getFieldArray (sfMajorities);
        for (auto const& majority : oldMajorities)
        {
            if (majority.getFieldH256 (sfAmendment) == amendment)
            {
                if (gotMajority)
                    return tefALREADY;
                found = true;
            }
            else
            {
                // pass through
                newMajorities.push_back (majority);
            }
        }
    }

    if (! found && lostMajority)
        return tefALREADY;

    if (gotMajority)
    {
        // This amendment now has a majority
        newMajorities.push_back (STObject (sfMajority));
        auto& entry = newMajorities.back ();
        entry.emplace_back (STHash256 (sfAmendment, amendment));
        entry.emplace_back (STUInt32 (sfCloseTime,
            view().parentCloseTime().time_since_epoch().count()));

        if (!ctx_.app.getAmendmentTable ().isSupported (amendment))
        {
            JLOG (j_.warn()) <<
                "Unsupported amendment " << amendment <<
                " received a majority.";
        }
    }
    else if (!lostMajority)
    {
        // No flags, enable amendment
        amendments.push_back (amendment);
        amendmentObject->setFieldV256 (sfAmendments, amendments);

        ctx_.app.getAmendmentTable ().enable (amendment);

        if (!ctx_.app.getAmendmentTable ().isSupported (amendment))
        {
            JLOG (j_.error()) <<
                "Unsupported amendment " << amendment <<
                " activated: server blocked.";
            ctx_.app.getOPs ().setAmendmentBlocked ();
        }
    }

    if (newMajorities.empty ())
        amendmentObject->makeFieldAbsent (sfMajorities);
    else
        amendmentObject->setFieldArray (sfMajorities, newMajorities);

    view().update (amendmentObject);

    return tesSUCCESS;
}

TER
Change::applyFee()
{
    auto const k = keylet::fees();

    SLE::pointer feeObject = view().peek (k);

    if (!feeObject)
    {
        feeObject = std::make_shared<SLE>(k);
        view().insert(feeObject);
    }

    feeObject->setFieldU64 (
        sfBaseFee, ctx_.tx.getFieldU64 (sfBaseFee));
    feeObject->setFieldU32 (
        sfReferenceFeeUnits, ctx_.tx.getFieldU32 (sfReferenceFeeUnits));
    feeObject->setFieldU32 (
        sfReserveBase, ctx_.tx.getFieldU32 (sfReserveBase));
    feeObject->setFieldU32 (
        sfReserveIncrement, ctx_.tx.getFieldU32 (sfReserveIncrement));

    view().update (feeObject);

    JLOG(j_.warn()) << "Fees have been changed";
    return tesSUCCESS;
}

TER Change::applyCRN_Round()
{
    auto const k = keylet::crnRound();

    SLE::pointer crnRoundObject = view().peek(k);

    if (!crnRoundObject)
    {
        crnRoundObject = std::make_shared<SLE>(k);
        view().insert(crnRoundObject);
    }

//    crnRoundObject->
    // jrojek TODO: evaluate CRN round object to apply it.
    JLOG(j_.warn()) << "CRN Round have concluded and is applied (ok, it's not, but will be soon";

    return tesSUCCESS;
}

TER Change::applyCRN_Report()
{
    JLOG(j_.info()) << "applyCRN_Report!";
    AccountID const crnAccountID (ctx_.tx.getAccountID (sfAccount));

    // Open a ledger for editing.
    SLE::pointer sleAcc = view().peek (keylet::account(crnAccountID));

    if (!sleAcc)
    {
        JLOG(j_.warn()) << "Sourceccount does not exist. Kind of impossible";
        return temDST_NEEDED;
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

    if (!(uFlagsIn & lsfKYCValidated))
    {
        JLOG(j_.warn()) << "Account is not KYC validated. Please fill KYC first";
        return temINVALID;
    }
    auto crnObject = sleAcc->peekFieldObject(sfCRN);

    // PubKey
    if (crnObject.isFieldPresent(sfCRN_PublicKey))
    {
        if (makeSlice(crnObject.getFieldVL(sfCRN_PublicKey)) != makeSlice(ctx_.tx.getFieldVL(sfCRN_PublicKey)))
        {
            JLOG(j_.warn()) << "Public Key mismatch. Should actually blacklist this node";
            return temMALFORMED;
        }
    }
    else
        crnObject.setFieldVL(sfCRN_PublicKey, makeSlice(ctx_.tx.getFieldVL(sfCRN_PublicKey)));

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

    STArray connStats (sfCRN_ConnectionStats);
    // jrojek TODO
//    connStats.push_back(STObject (sfCRN_ConnectionStat));
    auto& entry = connStats.back();
//    entry.emplace_back (STUInt8 (sfConnType, abc));
//    entry.emplace_back (STUInt32 (sfTime, def));
    crnObject.setFieldArray(sfCRN_ConnectionStats, connStats);

// jrojek TODO
//    if (uFlagsIn != uFlagsOut)
//        sleAcc->setFieldU32 (sfFlags, uFlagsOut);


    JLOG(j_.warn()) << "CRN Report for account: " << toBase58(account_) << " is applied (it is not, but will be soon)";
    return tesSUCCESS;
}

}
