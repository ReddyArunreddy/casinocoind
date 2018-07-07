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

void TMDFSReportStateData::restartTimers(std::string const& initiatorPubKey,
                                        std::string const& currRecipient,
                                        protocol::TMDFSReportState const& currPayload)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::restartACKTimer()"
                          << "initiator: " << initiatorPubKey
                          << " curr recipient: " << currRecipient;
    std::lock_guard<decltype(mutex_)> lock(mutex_);

//    if (ackTimers_.find(initiatorPubKey) == ackTimers_.end())
    ackTimers_[initiatorPubKey] = std::make_unique<DeadlineTimer>(this);

//    if (responseTimers_.find(initiatorPubKey) == responseTimers_.end())
    responseTimers_[initiatorPubKey] = std::make_unique<DeadlineTimer>(this);

    ackTimers_[initiatorPubKey]->setExpiration(1s);
    responseTimers_[initiatorPubKey]->setExpiration(20s);

    lastReqRecipient_[initiatorPubKey] = currRecipient;
    lastReq_[initiatorPubKey] = currPayload;
}

void TMDFSReportStateData::cancelTimer(std::string const& initiatorPubKey, TimerType type)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::cancelTimer() "
                          << "initiator: " << initiatorPubKey;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    cancelTimer_private(initiatorPubKey, type);
}

protocol::TMDFSReportState& TMDFSReportStateData::getLastRequest(std::string const& initiatorPubKey)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::getLastRequest() "
                          << "initiator: " << initiatorPubKey;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    return lastReq_[initiatorPubKey];
}

std::string& TMDFSReportStateData::getLastRecipient(std::string const& initiatorPubKey)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::getLastRecipient() "
                          << "initiator: " << initiatorPubKey;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    return lastReqRecipient_[initiatorPubKey];
}

void TMDFSReportStateData::conclude(const std::string &initiatorPubKey)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::conclude() "
                          << "initiator: " << initiatorPubKey;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    ackTimers_[initiatorPubKey].reset();
    responseTimers_[initiatorPubKey].reset();

    lastReqRecipient_[initiatorPubKey] = std::string();
    lastReq_[initiatorPubKey] = protocol::TMDFSReportState();
}

void TMDFSReportStateData::onDeadlineTimer(DeadlineTimer &timer)
{
    // jrojek this might be because node just recently gone offline
    // or because node does not support CRN feature. Either way, we decide that this node is already
    // visited and do not account its state
    protocol::TMDFSReportState msgToSend;
    std::string recipient;
    JLOG(journal_.debug()) << "TMDFSReportStateData::onDeadlineTimer()";
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        std::string initiator;
        // try to map to initiator using ACK timers
        for (auto iter = ackTimers_.begin(); iter != ackTimers_.end(); ++iter)
        {
            if (*(iter->second) == timer)
            {
                initiator = iter->first;
                JLOG(journal_.debug()) << "TMDFSReportStateData::onDeadlineTimer() ACK timer for initiator: " << initiator;
                cancelTimer_private(initiator, RESPONSE_TIMER);
                break;
            }
        }
        if (initiator.empty())
        {
            for (auto iter = responseTimers_.begin(); iter != responseTimers_.end(); ++iter)
            {
                if (*(iter->second) == timer)
                {
                    initiator = iter->first;
                    JLOG(journal_.debug()) << "TMDFSReportStateData::onDeadlineTimer() RESPONSE timer for initiator: " << initiator;
                    break;
                }
            }
        }
        if (initiator.empty())
        {
            JLOG(journal_.error()) << "TMDFSReportStateData::onDeadlineTimer() couldn't find corresponding timer. honestly don't know what to do";
            return;
        }
        lastReq_[initiator].add_visited(lastReqRecipient_[initiator]);
        msgToSend = lastReq_[initiator];
        recipient = lastReqRecipient_[initiator];
    }

    Overlay::PeerSequence knownPeers = overlay_.getSanePeers();
    if (knownPeers.size() > 0)
    // jrojek need to call that on any instance of TMDFSReportState as this is basically callback to 'me'
    {
        knownPeers[0]->dfsReportState().addTimedOutNode(std::make_shared<protocol::TMDFSReportState>(msgToSend), recipient);
    }
}

void TMDFSReportStateData::cancelTimer_private(const std::string &initiatorPubKey, TMDFSReportStateData::TimerType type)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::cancelTimer_private() "
                          << "initiator: " << initiatorPubKey;
    if (type == ACK_TIMER)
    {
        if (ackTimers_.find(initiatorPubKey) != ackTimers_.end())
            ackTimers_[initiatorPubKey].reset();
        else
            JLOG(journal_.warn()) << "TMDFSReportStateData::cancelTimer_private couldn't find ACK_TIMER "
                                  << "for root node: " << initiatorPubKey;
    }
    else if (type == RESPONSE_TIMER)
    {
        if (responseTimers_.find(initiatorPubKey) != responseTimers_.end())
            responseTimers_[initiatorPubKey].reset();
        else
            JLOG(journal_.warn()) << "TMDFSReportStateData::cancelTimer_private couldn't find RESPONSE_TIMER "
                                  <<"for root node: " << initiatorPubKey;
    }
}

}
