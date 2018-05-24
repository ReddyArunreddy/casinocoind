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
#include <casinocoin/overlay/Overlay.h>
#include <casinocoin/overlay/impl/PeerImp.h>
#include <casinocoin/overlay/Message.h>
#include <casinocoin/overlay/predicates.h>
#include <casinocoin/app/misc/CRNPerformance.h>
#include <casinocoin/app/misc/Transaction.h>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <algorithm>
#include <mutex>

namespace casinocoin {

class CRNPerformanceImpl final
    : public CRNPerformance
{
public:
    CRNPerformanceImpl (NetworkOPs& networkOps,
                        LedgerIndex const& startupSeq,
                        PublicKey const& crnPubKey,
                        beast::Journal journal);

    Json::Value getJson () override;
    void submit (std::shared_ptr<ReadView const> const& lastClosedLedger,
                Application& app) override;

protected:
    NetworkOPs& networkOps;
    LedgerIndex lastSnapshotSeq_;
    PublicKey crnPubKey_;
    beast::Journal j_;
    std::array<PeerImp::StatusAccounting::Counters,5> lastSnapshot_;

private:
    std::array<PeerImp::StatusAccounting::Counters,5>
    mapServerAccountingToPeerAccounting(std::array<NetworkOPs::StateAccounting::Counters, 5> const& serverAccounting);
};

CRNPerformanceImpl::CRNPerformanceImpl(
        NetworkOPs& networkOps,
        LedgerIndex const& startupSeq,
        PublicKey const& crnPubKey,
        beast::Journal journal)
    : networkOps(networkOps)
    , lastSnapshotSeq_(startupSeq)
    , crnPubKey_(crnPubKey)
    , j_(journal)
{
    for (auto& oneCounter : lastSnapshot_)
    {
        oneCounter.dur = std::chrono::seconds(0);
        oneCounter.transitions = 0;
    }
}

Json::Value CRNPerformanceImpl::getJson()
{
    Json::Value ret;
    return ret;
}

void CRNPerformanceImpl::submit(std::shared_ptr<ReadView const> const& lastClosedLedger, Application& app)
{
    // LCL must be 'reporting' ledger
    assert ((lastClosedLedger->info().seq % getReportingPeriod()) == 0);

    protocol::NodeStatus currentStatus = networkOps.getNodeStatus();
    std::array<PeerImp::StatusAccounting::Counters, 5> counters =
            mapServerAccountingToPeerAccounting(networkOps.getServerAccountingInfo());

    protocol::TMReportState s;

    for (uint32_t i = 0; i < 5; i++)
    {
        PeerImp::StatusAccounting::Counters counterToReport;
        counterToReport.dur = std::chrono::duration_cast<std::chrono::seconds>(counters[i].dur - lastSnapshot_[i].dur);
        counterToReport.transitions = counters[i].transitions - lastSnapshot_[i].transitions;

        JLOG(j_.info()) << "CRNPerformanceImpl::submit TMReportState: "
                        << " mode: " << i+1
                        << " lastSnapshot   [" << i << "]dur:" << lastSnapshot_[i].dur.count() << " transitions:" << lastSnapshot_[i].transitions
                        << " counters       [" << i << "]dur:" << counters[i].dur.count() << " transitions:" << counters[i].transitions
                        << " counterToReport[" << i << "]dur:" << counterToReport.dur.count() << " transitions:" << counterToReport.transitions;


        lastSnapshot_[i].dur = counters[i].dur;
        lastSnapshot_[i].transitions = counters[i].transitions;

        protocol::TMReportState::Status* newStatus = s.add_status ();
        newStatus->set_mode(static_cast<protocol::NodeStatus>(i+1));
        newStatus->set_duration(counterToReport.dur.count());
        newStatus->set_transitions(counterToReport.transitions);
    }
    s.set_currstatus(currentStatus);
    s.set_ledgerseqbegin(lastSnapshotSeq_);
    s.set_ledgerseqend(lastClosedLedger->info().seq);

    auto const pk = crnPubKey_.slice();
    s.set_crnpubkey(pk.data(), pk.size());

    // jrojek TODO real latency... :O
    s.set_latency(10);

    app.overlay ().foreach (send_always (
        std::make_shared<Message> (s, protocol::mtREPORT_STATE)));

    lastSnapshotSeq_ = lastClosedLedger->info().seq;
}

std::array<PeerImp::StatusAccounting::Counters, 5> CRNPerformanceImpl::mapServerAccountingToPeerAccounting(std::array<NetworkOPs::StateAccounting::Counters, 5> const& serverAccounting)
{
    JLOG(j_.info()) << "CRNPerformanceImpl::mapServerAccountingToPeerAccounting";

    std::array<PeerImp::StatusAccounting::Counters, 5> ret;
    for (uint32_t i = 0; i < 5; i++)
    {
        auto &entry = ret[(networkOps.getNodeStatus(static_cast<NetworkOPs::OperatingMode>(i))) - 1];
        entry.dur += std::chrono::seconds(serverAccounting[i].dur.count() / 1000 / 1000);
        entry.transitions += serverAccounting[i].transitions;
        JLOG(j_.info()) << "CRNPerformanceImpl::mapServerAccountingToPeerAccounting TMReportState:"
                        << " OperatingMode(" << i << ") -> protocol::NodeStatus(" << networkOps.getNodeStatus(static_cast<NetworkOPs::OperatingMode>(i)) << ")"
                        << " serverAccounting[" << i << "]dur:" << serverAccounting[i].dur.count() << " transitions:" << serverAccounting[i].transitions
                        << " ret             [" << (networkOps.getNodeStatus(static_cast<NetworkOPs::OperatingMode>(i))) - 1 << "]dur:" << entry.dur.count() << " transitions:" << entry.transitions
                        ;
    }
    return ret;
}


std::unique_ptr<CRNPerformance> make_CRNPerformance(
    NetworkOPs& networkOps,
    LedgerIndex const& startupSeq,
    PublicKey const& crnPubKey,
    beast::Journal journal)
{
    return std::make_unique<CRNPerformanceImpl> (
                networkOps,
                startupSeq,
                crnPubKey,
                journal);
}

}
