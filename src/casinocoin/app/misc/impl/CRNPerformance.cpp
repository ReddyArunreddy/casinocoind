//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/casinocoin/casinocoind
    Copyright (c) 2018 CasinoCoin Foundation

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
    2018-05-15  jrojek          Created
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/app/ledger/OpenLedger.h>
#include <casinocoin/app/main/Application.h>
#include <casinocoin/app/misc/NetworkOPs.h>
#include <casinocoin/app/misc/CRNPerformance.h>
#include <casinocoin/core/ConfigSections.h>
#include <casinocoin/protocol/JsonFields.h>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <algorithm>
#include <mutex>

namespace casinocoin {

class CRNPerformanceImpl final
    : public CRNPerformance
{
public:
    CRNPerformanceImpl (
        NetworkOPs& networkOps,
        AccountID const& account,
        beast::Journal journal);

    Json::Value getJson () override;
    void submit (std::shared_ptr<ReadView const> const& lastClosedLedger,
                std::shared_ptr<SHAMap> const& initialPosition) override;

protected:
    NetworkOPs& networkOps;
    AccountID account_;
    beast::Journal j_;
};

CRNPerformanceImpl::CRNPerformanceImpl(
        NetworkOPs& networkOps,
        AccountID const& account,
        beast::Journal journal)
    : networkOps(networkOps)
    , account_(account)
    , j_(journal)
{

}

Json::Value CRNPerformanceImpl::getJson()
{
    Json::Value ret;
    return ret;
}

void CRNPerformanceImpl::submit(std::shared_ptr<ReadView const> const& lastClosedLedger,
                               std::shared_ptr<SHAMap> const& initialPosition)
{
    // LCL must be flag ledger
    assert ((lastClosedLedger->info().seq % 256) == 0);
    auto const seq = lastClosedLedger->info().seq + 1;

    std::array<NetworkOPs::StateAccounting::Counters> counters = networkOps.getServerAccountingInfo();
    std::uint32_t latencyAvg = 123;

    JLOG(j_.warn()) << "submitting CRNReport tx";

    STTx crnReportTx (ttCRN_REPORT,
        [seq,counters,latencyAvg](auto& obj)
        {
            obj[sfAccount] = account_;
            obj[sfFee] = beast::zero;
//            obj[sfSigningPubKey] =
            obj[sfLedgerSequence] = seq;
            obj[sfCRN_PublicKey] = Blob();
            obj[sfCRN_IPAddress] = Blob();
            obj[sfCRN_DomainName] = Blob();
            obj[sfCRN_LatencyAvg] = latencyAvg;
            obj[sfCRN_ConnectionStats] = Blob();
        });

    uint256 txID = crnReportTx.getTransactionID ();

    JLOG(journal_.warn()) <<
        "CRNReport for account: " << toBase58(account_) << " txID: " txID;

    Serializer s;
    crnReportTx.add (s);

    auto tItem = std::make_shared<SHAMapItem> (txID, s.peekData ());

    if (!initialPosition->addGiveItem (tItem, true, false))
    {
        JLOG(journal_.warn()) <<
            "Ledger already had CRNReport for account: " << toBase58(account_);
    }

}


std::unique_ptr<CRNPerformance> make_CRNPerformance(
    NetworkOPs& networkOps,
    AccountID const& account,
    beast::Journal journal)
{
    return std::make_unique<CRNPerformanceImpl> (
                networkOps,
                account,
                journal);
}

}
