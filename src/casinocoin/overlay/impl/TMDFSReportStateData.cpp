//------------------------------------------------------------------------------
/*
    This file is part of casinocoind: https://github.com/casinocoin/casinocoind
    Copyright (c) 2018 Casinocoin Foundation

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
    2018-06-04  jrojek        created
*/
//==============================================================================

#include "TMDFSReportStateData.h"
#include <casinocoin/overlay/impl/OverlayImpl.h>

namespace casinocoin {

TMDFSReportStateData::TMDFSReportStateData(OverlayImpl& overlay,
                                           beast::Journal journal)
    : overlay_(overlay)
    , journal_(journal)
{
}

void TMDFSReportStateData::restartTimer(std::string const& initiatorPubKey,
                                                    std::string const& currRecipient,
                                                    protocol::TMDFSReportState const& currPayload)
{
    JLOG(journal_.info()) << "TMDFSReportStateData::restartTimer for root node " << initiatorPubKey;

    std::lock_guard<decltype(mutex_)> lock(mutex_);

    JLOG(journal_.info()) << "TMDFSReportStateData::restartTimer list of existing timers:";
    for (auto iter = dfsTimers_.begin(); iter != dfsTimers_.end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportStateData::restartTimer:" << iter->first;

    if (dfsTimers_.find(initiatorPubKey) == dfsTimers_.end())
        dfsTimers_[initiatorPubKey] = std::make_unique<DeadlineTimer>(this);

    dfsTimers_[initiatorPubKey]->setExpiration(1s);
    lastReqRecipient_[initiatorPubKey] = currRecipient;
    lastReq_[initiatorPubKey] = currPayload;
}

void TMDFSReportStateData::cancelTimer(std::string const& initiatorPubKey)
{
    JLOG(journal_.info()) << "TMDFSReportStateData::cancelTimer root node " << initiatorPubKey;
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    JLOG(journal_.info()) << "TMDFSReportStateData::cancelTimer list of existing timers:";
    for (auto iter = dfsTimers_.begin(); iter != dfsTimers_.end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportStateData::cancelTimer:" << iter->first;

    if (dfsTimers_.find(initiatorPubKey) != dfsTimers_.end())
        dfsTimers_[initiatorPubKey]->cancel();
    else
        JLOG(journal_.info()) << "TMDFSReportStateData::cancelTimer couldn't find timer for root node: " << initiatorPubKey;
}

protocol::TMDFSReportState& TMDFSReportStateData::getLastRequest(std::string const& initiatorPubKey)
{
    return lastReq_[initiatorPubKey];
}

std::string& TMDFSReportStateData::getLastRecipient(std::string const& initiatorPubKey)
{
    return lastReqRecipient_[initiatorPubKey];
}

void TMDFSReportStateData::onDeadlineTimer(DeadlineTimer &timer)
{
    JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer";
    // jrojek this might be because node just recently gone offline
    // or because node does not support CRN feature. Either way, we decide that this node is already
    // visited and do not account its state
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    timer.cancel();

    std::string initiator;
    for (auto iter = dfsTimers_.begin(); iter != dfsTimers_.end(); ++iter)
    {
        if (*(iter->second) == timer)
        {
            initiator = iter->first;
            break;
        }
    }
    JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer initiator: " << initiator;
    JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer lastReqRecipient_: " << lastReqRecipient_[initiator];
    lastReq_[initiator].add_visited(lastReqRecipient_[initiator]);
    lastReq_[initiator].set_type(protocol::TMDFSReportState::rtRESP);

    JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer statecheck: visited " << lastReq_[initiator].visited_size() << " reports: " << lastReq_[initiator].reports_size() << " dfs: " << lastReq_[initiator].dfs_size();
    auto visitedListCheck = lastReq_[initiator].mutable_visited();
    for ( auto iter = visitedListCheck->begin(); iter != visitedListCheck->end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer statecheck visited nodes " << *iter;
    auto dfsListCheck = lastReq_[initiator].mutable_dfs();
    for ( auto iter = dfsListCheck->begin(); iter != dfsListCheck->end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer dfs nodes " << *iter;


    Overlay::PeerSequence knownPeers = overlay_.getActivePeers();
    if (knownPeers.size() > 0)
    {
        knownPeers[0]->dfsReportState().evaluateResponse(std::make_shared<protocol::TMDFSReportState>(lastReq_[initiator]));
//        for (auto const& singlePeer : knownPeers)
//        {
//            JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer peer " << toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
//            if (toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()) == lastReq_[initiator].dfs(lastReq_[initiator].dfs_size() - 1))
//            {
//                JLOG(journal_.info()) << "TMDFSReportStateData::onDeadlineTimer imitating response to: " << toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
//                singlePeer->send(std::make_shared<Message>(lastReq_[initiator], protocol::mtDFS_REPORT_STATE));
//                break;
//            }
//        }
    }
    lastReq_[initiator].set_type(protocol::TMDFSReportState::rtREQ);
}

}
