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
                                   boost::asio::io_service& io_service,
                                   PeerImp& parent,
                                   beast::Journal journal)
    : app_(app)
    , overlay_(overlay)
    , parentPeer_(parent)
    , journal_(journal)
    , pubKeyString_(toBase58(TOKEN_NODE_PUBLIC, app_.nodeIdentity().first))
    , strand_(io_service)
    , io_service_(io_service)
{
    JLOG(journal_.info()) << "TMDFSReportState::TMDFSReportState() created for " << pubKeyString_ << " peer POV for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());
}

bool TMDFSReportState::start()
{
    if (isStarted_)
    {
        JLOG(journal_.warn()) << "TMDFSReportState::start() is already started at " << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start_).count();
        return false;
    }

    reset();

    if (app_.isCRN())
    {
        reportState_.insert(std::pair<std::string, protocol::TMReportState>(
                                pubKeyString_,
                                app_.getCRN().performance().getPreparedReport()));
    }

    visited_.insert(std::pair<std::string, bool>(pubKeyString_, true));
    dfs_.push_back(pubKeyString_);

    // msg
    protocol::TMDFSReportState msg;
    if (app_.isCRN())
    {
        protocol::TMDFSReportState::PubKeyReportMap* newEntry = msg.add_reports ();
        newEntry->set_pubkey(pubKeyString_);
        newEntry->set_allocated_report(std::make_shared<protocol::TMReportState>(app_.getCRN().performance().getPreparedReport()).get());
    }
    msg.add_visited(pubKeyString_);
    msg.set_type(protocol::TMDFSReportState::rtREQ);

    parentPeer_.send(std::make_shared<Message>(msg, protocol::mtDFS_REPORT_STATE));

    return true;

//    Overlay::PeerSequence knownPeers = overlay_.getActivePeers();
//    if (knownPeers.size() > 0)
//    {
//        lastReq_ = msg;
//        lastReqRecipient_ = toBase58(TOKEN_NODE_PUBLIC, knownPeers[0]->getNodePublic());
//        setTimer(lastReqRecipient_);
//        overlay_.foreach (send_if (
//            std::make_shared<Message>(msg, protocol::mtDFS_REPORT_STATE_REQ),
//            match_peer(knownPeers[0].get())));
//    }
//    else
//    {
//        JLOG(journal_.warn()) << "TMDFSReportState::start() "
//                              << "Something went terribly wrong, no active peers discovered";
//    }
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

    protocol::TMDFSReportState forwardMsg = *m;
    if (app_.isCRN())
    {
        protocol::TMDFSReportState::PubKeyReportMap* newEntry = forwardMsg.add_reports ();
        newEntry->set_pubkey(pubKeyString_);
        newEntry->set_allocated_report(std::make_shared<protocol::TMReportState>(app_.getCRN().performance().getPreparedReport()).get());
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

            lastReq_ = forwardMsg;
            lastReqRecipient_ = toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
            setTimer(lastReqRecipient_);
            overlay_.foreach (send_if (
                                  std::make_shared<Message>(forwardMsg, protocol::mtDFS_REPORT_STATE),
                                  match_peer(singlePeer.get())));

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


    // jrojek TODO: On request, need to add current node to the lists,
    //              check for first peer which is not already on the list
    //              and call request on that peer
    //              when all peers are on the list already, return Response to caller
    // open point: how to determine the caller? in a message! (last entry in dfs list)
}

void TMDFSReportState::evaluateResponse(const std::shared_ptr<protocol::TMDFSReportState> &m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());

    if (m->type() != protocol::TMDFSReportState::rtRESP)
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateResponse() TMDFSReportState evaluating resp but it is not a resp";
        return;
    }

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
            if (alreadyVisited)
                continue;

            m->set_type(protocol::TMDFSReportState::rtREQ);
            lastReq_ = *m;
            lastReqRecipient_ = toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic());
            setTimer(lastReqRecipient_);
            overlay_.foreach (send_if (
                                  std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE),
                                  match_peer(singlePeer.get())));

            return;
        }
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportState::start() "
                              << "Something went terribly wrong, no active peers discovered";

        return;
    }

    parentPeer_.send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));
    // jrojek TODO: when response is received, need to check if there are still
    //              peers on our list who were not visited, and call request on them, if not
    //              call Response on caller
}

void TMDFSReportState::evaluateAck(const std::shared_ptr<protocol::TMDFSReportStateAck> &m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateAck TMDFSReportStateAck for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());
    cancelTimer(toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()));
}

void TMDFSReportState::reset()
{
    reportState_.clear();
    visited_.clear();
    dfs_.clear();
}

void TMDFSReportState::setTimer( std::string const& pubKeyString)
{
    JLOG(journal_.info()) << "TMDFSReportState::setTimer for node " << pubKeyString;
    error_code ec;
    timers_[pubKeyString] = std::make_unique<boost::asio::basic_waitable_timer<std::chrono::steady_clock>>(io_service_);
    timers_[pubKeyString]->expires_from_now(std::chrono::seconds(2), ec);
    if (ec)
    {
        JLOG(journal_.error()) <<
            "setTimer: " << ec.message();
        return;
    }

    timers_[pubKeyString]->async_wait(strand_.wrap(std::bind(
        &TMDFSReportState::onTimer, shared_from_this(),
            beast::asio::placeholders::error)));
}

void TMDFSReportState::cancelTimer(const std::string &pubKeyString)
{
    JLOG(journal_.info()) << "TMDFSReportState::cancelTimer node " << pubKeyString;
    error_code ec;
    timers_[pubKeyString]->cancel(ec);
    timers_.erase(pubKeyString);
}

void TMDFSReportState::onTimer(error_code ec)
{
    JLOG(journal_.warn()) << "TMDFSReportState::onTimer node " << lastReqRecipient_<< " didn't ACK in timely manner";
    // jrojek this might be because node just recently gone offline
    // or because node does not support CRN feature. Either way, we decide that this node is already
    // visited and do not account its state
    if (ec)
    {
        JLOG(journal_.error()) <<
            "TMDFSReportState::onTimer: " << ec.message();
    }
    timers_.erase(lastReqRecipient_);

    lastReq_.add_visited(lastReqRecipient_);
    lastReq_.set_type(protocol::TMDFSReportState::rtRESP);
    evaluateResponse(std::make_shared<protocol::TMDFSReportState>(lastReq_));
}

} // namespace casinocoin

