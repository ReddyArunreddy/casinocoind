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
    2018-05-30  jrojek        created
*/
//==============================================================================

#include <BeastConfig.h>

#include <casinocoin/overlay/impl/TMDFSReportState.h>
#include <casinocoin/overlay/impl/PeerImp.h>
#include <casinocoin/app/misc/NetworkOPs.h>

namespace casinocoin {

TMDFSReportState::TMDFSReportState(Application& app,
                                   OverlayImpl& overlay,
                                   PeerImp& parent,
                                   beast::Journal journal)
    : app_(app)
    , overlay_(overlay)
    , parentPeer_(parent)
    , journal_(journal)
    , pubKeyString_(toBase58(TOKEN_NODE_PUBLIC, app_.nodeIdentity().first))
{
    JLOG(journal_.info()) << "TMDFSReportState::TMDFSReportState() created for " << pubKeyString_ << " peer POV for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());
}

TMDFSReportState::~TMDFSReportState()
{
    JLOG(journal_.info()) << "TMDFSReportState::~TMDFSReportState() removed for " << pubKeyString_ << " peer POV for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());
}

bool TMDFSReportState::start()
{
    if (isStarted_)
    {
        JLOG(journal_.warn()) << "TMDFSReportState::start() is already started at " << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start_).count();
        return false;
    }

    JLOG(journal_.info()) << "TMDFSReportState::start() set ME " << pubKeyString_ << " send req to: " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());

    // msg
    protocol::TMDFSReportState msg;
    if (app_.isCRN())
    {
        protocol::TMDFSReportState::PubKeyReportMap* newEntry = msg.add_reports ();
        newEntry->set_pubkey(pubKeyString_);
        newEntry->set_allocated_report( new protocol::TMReportState(app_.getCRN().performance().getPreparedReport()));
    }
    msg.add_visited(pubKeyString_);
    msg.add_dfs(pubKeyString_);
    msg.set_type(protocol::TMDFSReportState::rtREQ);

    lastReq_ = msg;
    lastReqRecipient_ = toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());
    overlay_.addDFSReportTimer(lastReqRecipient_, this);

    parentPeer_.send(std::make_shared<Message>(msg, protocol::mtDFS_REPORT_STATE));

    isStarted_ = true;

    // jrojek TODO: add whole crawl timeout
    return true;
}

void TMDFSReportState::evaluateRequest(std::shared_ptr<protocol::TMDFSReportState> const& m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest() TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());

    if (m->type() != protocol::TMDFSReportState::rtREQ)
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateRequest() TMDFSReportState evaluating req but it is not a req";
        return;
    }

    for (std::string const& visitedNode : m->visited())
    {
        if (visitedNode == pubKeyString_)
        {
            JLOG(journal_.error()) << "TMDFSReportState::evaluateRequest() TMDFSReportState received Req in a node which is already on the list! " << pubKeyString_;
            return;
        }
    }

    JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest() statecheck: visited " << m->visited_size() << " reports: " << m->reports_size() << " dfs: " << m->dfs_size();
    auto visitedListCheck = m->mutable_visited();
    for ( auto iter = visitedListCheck->begin(); iter != visitedListCheck->end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest() statecheck visited nodes " << *iter;
    auto dfsListCheck = m->mutable_dfs();
    for ( auto iter = dfsListCheck->begin(); iter != dfsListCheck->end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest() dfs nodes " << *iter;


    protocol::TMDFSReportState forwardMsg = *m;
    if (app_.isCRN())
    {
        protocol::TMDFSReportState::PubKeyReportMap* newEntry = forwardMsg.add_reports ();
        newEntry->set_pubkey(pubKeyString_);
        newEntry->set_allocated_report( new protocol::TMReportState(app_.getCRN().performance().getPreparedReport()));
    }
    forwardMsg.add_visited(pubKeyString_);

    Overlay::PeerSequence knownPeers = overlay_.getActivePeers();
    if (knownPeers.size() > 0)
    {
        for (auto const& singlePeer : knownPeers)
        {
            bool alreadyVisited = false;
            std::string singlePeerPubKeyString = toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
            for (std::string const& visitedNode : forwardMsg.visited())
            {
                if (singlePeerPubKeyString == visitedNode)
                {
                    alreadyVisited = true;
                    break;
                }
            }
            if (alreadyVisited)
                continue;

            forwardMsg.set_type(protocol::TMDFSReportState::rtREQ);
            forwardMsg.add_dfs(pubKeyString_);

            lastReq_ = forwardMsg;
            lastReqRecipient_ = toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
            overlay_.addDFSReportTimer(lastReqRecipient_, this);
            singlePeer->send(std::make_shared<Message>(forwardMsg, protocol::mtDFS_REPORT_STATE));

            return;
        }
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportState::evaluateRequest() "
                              << "Something went terribly wrong, no active peers discovered";
        return;
    }

    // if we reach this point this means that we already visited all our peers and we know of their state
    forwardMsg.set_type(protocol::TMDFSReportState::rtRESP);

    parentPeer_.send(std::make_shared<Message>(forwardMsg, protocol::mtDFS_REPORT_STATE));
}

void TMDFSReportState::evaluateResponse(const std::shared_ptr<protocol::TMDFSReportState> &m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());

    if (m->type() != protocol::TMDFSReportState::rtRESP)
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateResponse() TMDFSReportState evaluating resp but it is not a resp";
        return;
    }

    JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() statecheck: visited " << m->visited_size() << " reports: " << m->reports_size() << " dfs: " << m->dfs_size();
    auto visitedListCheck = m->mutable_visited();
    for ( auto iter = visitedListCheck->begin(); iter != visitedListCheck->end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() statecheck visited nodes " << *iter;
    auto dfsListCheck = m->mutable_dfs();
    for ( auto iter = dfsListCheck->begin(); iter != dfsListCheck->end(); ++iter)
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() dfs nodes " << *iter;


    std::shared_ptr<Peer> storedLastReqRecipientPeer;
    Overlay::PeerSequence knownPeers = overlay_.getActivePeers();
    if (knownPeers.size() > 0)
    {
        for (auto const& singlePeer : knownPeers)
        {
            bool alreadyVisited = false;
            std::string singlePeerPubKeyString = toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
            for (std::string const& visitedNode : m->visited())
            {
                if (singlePeerPubKeyString == visitedNode)
                {
                    alreadyVisited = true;
                    break;
                }
            }
            if (toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()) == lastReqRecipient_)
                    storedLastReqRecipientPeer = singlePeer;
            if (alreadyVisited)
                continue;

            m->set_type(protocol::TMDFSReportState::rtREQ);
            lastReq_ = *m;
            lastReqRecipient_ = toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
            overlay_.addDFSReportTimer(lastReqRecipient_, this);
            singlePeer->send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));
            return;
        }
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportState::evaluateResponse() "
                              << "Something went terribly wrong, no active peers discovered";
        return;
    }

    JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() Crawl. dfs size == " << m->dfs_size();
    if (isStarted_)
    {
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() Crawl concluded. final stats: visited: " << m->visited_size() << " CRN nodes reported: " << m->reports_size();
        isStarted_ = false;
    }
    else
    {
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() Crawl locally concluded. current stats: visited: " << m->visited_size() << " CRN nodes reported: " << m->reports_size();

        auto dfsList = m->mutable_dfs();
        if (dfsList->size() > 0 && dfsList->Get(dfsList->size() - 1) == pubKeyString_)
        {
            JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() removing 'me' " << pubKeyString_ << " from DFS list";
            dfsList->RemoveLast();
        }
        else
        {
            JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() couldn't remove 'me' " << pubKeyString_ << " from DFS list";
        }
        if (dfsList->size() > 0)
        {
            for (auto const& singlePeer : knownPeers)
            {
                if (toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()) == dfsList->Get(dfsList->size() - 1))
                {
                    JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() found response endpoint: " << dfsList->Get(dfsList->size() - 1) << " responding!";
                    singlePeer->send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));
                    break;
                }
            }
        }
        else
        {
            JLOG(journal_.warn()) << "TMDFSReportState::evaluateResponse() crawl not concluded globally but dfs list exhausted";
        }
    }
}

void TMDFSReportState::evaluateAck(const std::shared_ptr<protocol::TMDFSReportStateAck> &m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateAck() TMDFSReportStateAck for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());
    overlay_.cancelDFSReportTimer(toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()));
}

void TMDFSReportState::onDeadlineTimer(DeadlineTimer &timer)
{
    JLOG(journal_.info()) << "TMDFSReportState::onDeadlineTimer";
    // jrojek this might be because node just recently gone offline
    // or because node does not support CRN feature. Either way, we decide that this node is already
    // visited and do not account its state
    overlay_.removeDFSReportTimer(lastReqRecipient_, timer);

    lastReq_.add_visited(lastReqRecipient_);
    lastReq_.set_type(protocol::TMDFSReportState::rtRESP);
    evaluateResponse(std::make_shared<protocol::TMDFSReportState>(lastReq_));
    JLOG(journal_.info()) << "TMDFSReportState::onDeadlineTimer quit fine";
}

} // namespace casinocoin

