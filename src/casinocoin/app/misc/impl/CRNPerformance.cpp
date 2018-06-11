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
#include <casinocoin/overlay/Message.h>
#include <casinocoin/overlay/predicates.h>
#include <casinocoin/app/misc/CRNPerformance.h>
#include <casinocoin/app/misc/CRNId.h>
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
                        CRNId const& crnId,
                        beast::Journal journal);

    StatusAccounting& accounting() override;
    Json::Value json () const override;

    protocol::TMReportState
    prepareReport (LedgerIndex const& lastClosedLedgerSeq,
                   Application& app) override;

    protocol::TMReportState const& getPreparedReport() const override;


    void broadcast (Application &app) override;

    bool onOverlayMessage(std::shared_ptr<protocol::TMReportState> const& m) override;

protected:
    NetworkOPs& networkOps;
    LedgerIndex lastSnapshotSeq_;
    CRNId const& id;
    beast::Journal j_;

    StatusAccounting accounting_;
    std::array<StatusAccounting::Counters,5> lastSnapshot_;
    std::array<StatusAccounting::Counters, 5> peerSelfAccounting_;
    uint32_t latency_;
    protocol::TMReportState preparedReport_;

private:
    void initializePerformanceReport();

    std::array<StatusAccounting::Counters,5>
    mapServerAccountingToPeerAccounting(std::array<NetworkOPs::StateAccounting::Counters, 5> const& serverAccounting);

    std::mutex mutable recentLock_;
};

CRNPerformanceImpl::CRNPerformanceImpl(
        NetworkOPs& networkOps,
        LedgerIndex const& startupSeq,
        CRNId const& crnId,
        beast::Journal journal)
    : networkOps(networkOps)
    , lastSnapshotSeq_(startupSeq)
    , id(crnId)
    , j_(journal)
    , accounting_(j_)
{
    initializePerformanceReport();
}

CRNPerformance::StatusAccounting &CRNPerformanceImpl::accounting()
{
    return accounting_;
}

Json::Value CRNPerformanceImpl::json() const
{
    // report this node measurement and self-measurement
    Json::Value ret = accounting_.json();
    for (std::underlying_type_t<protocol::NodeStatus> i = protocol::nsCONNECTING;
         i <= protocol::nsSHUTTING; ++i)
    {
        uint8_t index = i-1;
        auto& status = ret[StatusAccounting::statuses_[index]];
        status[jss::self_transitions] = peerSelfAccounting_[index].transitions;
        status[jss::self_duration_sec] = std::to_string (peerSelfAccounting_[index].dur.count());
    }

     // report self-measurement only
//    Json::Value ret;
//    for (std::underlying_type_t<protocol::NodeStatus> i = protocol::nsCONNECTING;
//        i <= protocol::nsSHUTTING; ++i)
//    {
//        uint8_t index = i-1;
//        ret[StatusAccounting::statuses_[index]] = Json::objectValue;
//        auto& status = ret[StatusAccounting::statuses_[index]];
//        status[jss::self_transitions] = peerSelfAccounting_[index].transitions;
//        status[jss::self_duration_sec] = std::to_string (peerSelfAccounting_[index].dur.count());
//    }
    return ret;
}

protocol::TMReportState
CRNPerformanceImpl::prepareReport (
        LedgerIndex const& lastClosedLedgerSeq, Application &app)
{
    std::lock_guard<std::mutex> sl(recentLock_);
    protocol::NodeStatus currentStatus = networkOps.getNodeStatus();
    std::array<StatusAccounting::Counters, 5> counters =
            mapServerAccountingToPeerAccounting(networkOps.getServerAccountingInfo());

    preparedReport_.clear_status();
    preparedReport_.clear_currstatus();
    preparedReport_.clear_ledgerseqbegin();
    preparedReport_.clear_ledgerseqend();
    preparedReport_.clear_crnpubkey();
    preparedReport_.clear_domain();
    preparedReport_.clear_signature();
    preparedReport_.clear_latency();
    preparedReport_.clear_activated();

    // ajochems: no connection status reporting for now
    // jrojek:  we need to report state for peer crawling procedure, but we can strip down
    //          message to minimum when broadcasting ;)
     for (uint32_t i = 0; i < 5; i++)
     {
         StatusAccounting::Counters counterToReport;
         counterToReport.dur = std::chrono::duration_cast<std::chrono::seconds>(counters[i].dur - lastSnapshot_[i].dur);
         counterToReport.transitions = counters[i].transitions - lastSnapshot_[i].transitions;

         lastSnapshot_[i].dur = counters[i].dur;
         lastSnapshot_[i].transitions = counters[i].transitions;

         protocol::TMReportState::Status* newStatus = preparedReport_.add_status ();
         newStatus->set_mode(static_cast<protocol::NodeStatus>(i+1));
         newStatus->set_duration(counterToReport.dur.count());
         newStatus->set_transitions(counterToReport.transitions);
     }
    preparedReport_.set_currstatus(currentStatus);
    preparedReport_.set_ledgerseqbegin(lastSnapshotSeq_);
    preparedReport_.set_ledgerseqend(lastClosedLedgerSeq);

    auto const pk = id.publicKey().slice();
    preparedReport_.set_crnpubkey(pk.data(), pk.size());
    preparedReport_.set_domain(id.domain());
    preparedReport_.set_activated(id.activated());
    // ajochems: hide the signiture from the public
    // preparedReport_.set_signature(id.signature());

    // jrojek TODO? for now latency reported is the minimum latency to sane peers
    // (since latency reported by Peer is already averaged from last 8 mesaurements
    uint32_t myLatency = std::numeric_limits<uint32_t>::max();
    {
        auto peerList = app.overlay().getActivePeers();
        for (auto const& peer : peerList)
        {
            if (peer->sanity() == Peer::Sanity::sane)
                myLatency = std::min(myLatency, peer->latency());
        }
    }
    latency_ = myLatency;
    preparedReport_.set_latency(myLatency);

    lastSnapshotSeq_ = lastClosedLedgerSeq;
    return preparedReport_;

}

protocol::TMReportState const& CRNPerformanceImpl::getPreparedReport() const
{
    std::lock_guard<std::mutex> sl(recentLock_);
    return preparedReport_;
}

void CRNPerformanceImpl::broadcast(Application &app)
{
    std::lock_guard<std::mutex> sl(recentLock_);
    protocol::TMReportState strippedStateReport = preparedReport_;
    strippedStateReport.clear_status();
    app.overlay ().foreach (send_always (
        std::make_shared<Message> (strippedStateReport, protocol::mtREPORT_STATE)));
}

bool CRNPerformanceImpl::onOverlayMessage(const std::shared_ptr<protocol::TMReportState> &m)
{
    std::lock_guard<std::mutex> sl(recentLock_);
//    if (m->status_size() != peerSelfAccounting_.size())
//    {
//        JLOG(j_.warn()) << "CRNPerformanceImpl::onOverlayMessage TMReportState: reported statuses count == " << m->status_size()
//                                << "  != status supported count == " << peerSelfAccounting_.size();
//        return false;
//    }

    latency_ = m->latency();
    for (int i = 0; i < m->status_size(); ++i)
    {
        const protocol::TMReportState::Status& singleStatus = m->status(i);
        uint32_t index = static_cast<uint32_t>(singleStatus.mode()) - 1;
        peerSelfAccounting_[index].dur = static_cast<std::chrono::seconds>(singleStatus.duration());
        peerSelfAccounting_[index].transitions = singleStatus.transitions();
//        JLOG(j_.info()) << "CRNPerformanceImpl::onOverlayMessage TMReportState: spent: " << peerSelfAccounting_[index].dur.count()
//                                << " with " << StatusAccounting::statuses_[index].c_str() << " status, "
//                                << "transitioned: " << peerSelfAccounting_[index].transitions << " times"
//                                << " latency = " << latency_;
    }
    return true;
}

std::array<CRNPerformance::StatusAccounting::Counters, 5> CRNPerformanceImpl::mapServerAccountingToPeerAccounting(std::array<NetworkOPs::StateAccounting::Counters, 5> const& serverAccounting)
{
    std::array<StatusAccounting::Counters, 5> ret;
    for (uint32_t i = 0; i < 5; i++)
    {
        auto &entry = ret[(networkOps.getNodeStatus(static_cast<NetworkOPs::OperatingMode>(i))) - 1];
        entry.dur += std::chrono::seconds(serverAccounting[i].dur.count() / 1000 / 1000);
        entry.transitions += serverAccounting[i].transitions;
    }
    return ret;
}

void CRNPerformanceImpl::initializePerformanceReport()
{
    for (uint32_t i = 0; i < 5; i++)
    {
        protocol::TMReportState::Status* newStatus = preparedReport_.add_status ();
        newStatus->set_mode(static_cast<protocol::NodeStatus>(i+1));
        newStatus->set_duration(0);
        newStatus->set_transitions(0);
    }
    protocol::NodeStatus currentStatus = networkOps.getNodeStatus();
    preparedReport_.set_currstatus(currentStatus);
    preparedReport_.set_ledgerseqbegin(lastSnapshotSeq_);
    preparedReport_.set_ledgerseqend(lastSnapshotSeq_);

    auto const pk = id.publicKey().slice();
    preparedReport_.set_crnpubkey(pk.data(), pk.size());
    preparedReport_.set_domain(id.domain());
    preparedReport_.set_signature(id.signature());
    preparedReport_.set_latency(10000);
    preparedReport_.set_activated(false);
}

//-------------------------------------------------------------------------------------
static std::array<char const*, 5> const statusNames {{
    "connecting",
    "connected",
    "monitoring",
    "validating",
    "shutting"}};

std::array<Json::StaticString const, 5> const
CRNPerformance::StatusAccounting::statuses_ = {{
    Json::StaticString(statusNames[0]),
    Json::StaticString(statusNames[1]),
    Json::StaticString(statusNames[2]),
    Json::StaticString(statusNames[3]),
    Json::StaticString(statusNames[4])}};

CRNPerformance::StatusAccounting::StatusAccounting(beast::Journal journal)
    : j_(journal)
{
    mode_ = protocol::nsCONNECTING;
    counters_[protocol::nsCONNECTING-1].transitions = 1;
    start_ = std::chrono::system_clock::now();
}

void CRNPerformance::StatusAccounting::reset()
{
    std::unique_lock<std::mutex> lock (mutex_);

    for (auto counter : counters_)
        counter.reset();
    start_ = std::chrono::system_clock::now();
}

void CRNPerformance::StatusAccounting::mode (protocol::NodeStatus nodeStatus)
{
    if (nodeStatus == mode_)
        return;

    JLOG(j_.info()) << "changing operating mode from " << mode_ << " to " << nodeStatus << " trans before: " << counters_[nodeStatus-1].transitions;
    auto now = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock (mutex_);
    ++counters_[nodeStatus-1].transitions;
    counters_[mode_-1].dur += std::chrono::duration_cast<
        std::chrono::seconds>(now - start_);

    JLOG(j_.info()) << "trans after: " << counters_[nodeStatus-1].transitions;
    mode_ = nodeStatus;
    start_ = now;
}

Json::Value CRNPerformance::StatusAccounting::json() const
{
    auto counters = snapshot();

    Json::Value ret = Json::objectValue;

    for (std::underlying_type_t<protocol::NodeStatus> i = protocol::nsCONNECTING;
        i <= protocol::nsSHUTTING; ++i)
    {
        uint8_t index = i-1;
        ret[statuses_[index]] = Json::objectValue;
        auto& status = ret[statuses_[index]];
        status[jss::transitions] = counters[index].transitions;
        status[jss::duration_sec] = std::to_string (counters[index].dur.count());
    }

    return ret;
}

std::array<CRNPerformance::StatusAccounting::Counters, 5> CRNPerformance::StatusAccounting::snapshot() const
{
    std::unique_lock<std::mutex> lock (mutex_);

    auto counters = counters_;
    auto const start = start_;
    auto const mode = mode_;

    lock.unlock();

    counters[mode-1].dur += std::chrono::duration_cast<
        std::chrono::seconds>(std::chrono::system_clock::now() - start);

    return counters;
}

//-------------------------------------------------------------------------------------


std::unique_ptr<CRNPerformance> make_CRNPerformance(NetworkOPs& networkOps,
    LedgerIndex const& startupSeq,
    CRNId const& crnId,
    beast::Journal journal)
{
    return std::make_unique<CRNPerformanceImpl> (
                networkOps,
                startupSeq,
                crnId,
                journal);
}

}
