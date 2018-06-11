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
#include <casinocoin/app/misc/CRNRound.h>

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
}

TMDFSReportState::~TMDFSReportState()
{
}

void TMDFSReportState::start()
{
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

    overlay_.getDFSReportStateData().restartTimers(pubKeyString_,
                                                  toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()),
                                                  msg);

    parentPeer_.send(std::make_shared<Message>(msg, protocol::mtDFS_REPORT_STATE));
    JLOG(journal_.info()) << "TMDFSReportState::start TMDFSReportState";
}

void TMDFSReportState::evaluateRequest(std::shared_ptr<protocol::TMDFSReportState> const& m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest() TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());

    protocol::TMDFSReportState forwardMsg = *m;
    if (forwardMsg.type() != protocol::TMDFSReportState::rtREQ)
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateRequest() "
                               << "TMDFSReportState evaluating req but it is not a req";
        // jrojek... need to protect this situation not to break whole crawl procedure somehow
        // jrojek... for now just retrieve 'valid state'
        forwardMsg.set_type(protocol::TMDFSReportState::rtREQ);
    }

    for (std::string const& visitedNode : m->visited())
    {
        if (visitedNode == pubKeyString_)
        {
            JLOG(journal_.error()) << "TMDFSReportState::evaluateRequest() "
                                   << "TMDFSReportState received Req in a node which is already on the list! " << pubKeyString_;
            // jrojek... ignore this case for now
        }
    }

    JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 1";
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
            JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 2";

            forwardMsg.set_type(protocol::TMDFSReportState::rtREQ);
            forwardMsg.add_dfs(pubKeyString_);

            overlay_.getDFSReportStateData().restartTimers(forwardMsg.dfs(0),
                                                          toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()),
                                                          forwardMsg);
            JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 3";

            singlePeer->send(std::make_shared<Message>(forwardMsg, protocol::mtDFS_REPORT_STATE));

            return;
        }
    }
    else
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateRequest() "
                               << "Something went terribly wrong, no active peers discovered";
        return;
    }

    // if we reach this point this means that we already visited all our peers and we know of their state
    forwardMsg.set_type(protocol::TMDFSReportState::rtRESP);

    JLOG(journal_.info()) << "TMDFSReportState::evaluateRequest TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 4";
    parentPeer_.send(std::make_shared<Message>(forwardMsg, protocol::mtDFS_REPORT_STATE));
}

void TMDFSReportState::evaluateResponse(const std::shared_ptr<protocol::TMDFSReportState> &m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());

    if (m->type() != protocol::TMDFSReportState::rtRESP)
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateResponse() TMDFSReportState evaluating resp but it is not a resp";
        // jrojek... need to protect this situation not to break whole crawl procedure somehow
        // jrojek... for now just retrieve 'valid state'
        m->set_type(protocol::TMDFSReportState::rtRESP);
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

            JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 1";
            m->set_type(protocol::TMDFSReportState::rtREQ);
            JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 1.1 dfsSize " << m->dfs_size();
            overlay_.getDFSReportStateData().cancelTimer(m->dfs(0), TMDFSReportStateData::RESPONSE_TIMER);
            overlay_.getDFSReportStateData().restartTimers(m->dfs(0),
                                                          toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()),
                                                          *m);

            JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 2";
            singlePeer->send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));
            return;
        }
    }
    else
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateResponse() "
                               << "Something went terribly wrong, no active peers discovered";
        return;
    }

    JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 3";
    auto dfsList = m->mutable_dfs();
    if (dfsList->size() > 0 && dfsList->Get(dfsList->size() - 1) == pubKeyString_)
    {
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 4";
        overlay_.getDFSReportStateData().cancelTimer(m->dfs(0), TMDFSReportStateData::RESPONSE_TIMER);
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 5";
        dfsList->RemoveLast();
    }
    else
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateResponse() couldn't remove 'me' "
                               << pubKeyString_ << " from DFS list";
        return;
    }
    if (dfsList->size() > 0)
    {
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 6";
        for (auto const& singlePeer : knownPeers)
        {
            if (toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()) == dfsList->Get(dfsList->size() - 1))
            {
                JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse TMDFSReportState for node " << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()) << " chkpt: 7";
                singlePeer->send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));
                break;
            }
        }
    }
    else
    {
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() Crawl concluded. dfs list empty. final stats: visited: " << m->visited_size() << " CRN nodes reported: " << m->reports_size() << " known peers count: " << knownPeers.size();
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() :::::::::::::::::::::::::::::::::::::::: VERBOSE PRINTOUT ::::::::::::::::::::::::::::::::::::::::";
        CRN::EligibilityMap eligibilityMap;

        for (auto iter = m->reports().begin() ; iter != m->reports().end() ; ++iter)
        {
            protocol::TMReportState const& rep = iter->report();
            JLOG(journal_.info()) << " currStatus " << rep.currstatus()
                                  << " ledgerSeqBegin " << rep.ledgerseqbegin()
                                  << " ledgerSeqEnd " << rep.ledgerseqend()
                                  << " latency " << rep.latency()
                                  << " crnPubKey " << toBase58(TOKEN_NODE_PUBLIC,PublicKey(Slice(rep.crnpubkey().data(), rep.crnpubkey().size())))
                                  << " domain " << rep.domain()
                                  << " signature " << rep.signature();
            for (auto iterStatuses = rep.status().begin() ; iterStatuses != rep.status().end() ; ++iterStatuses)
            {
                JLOG(journal_.info()) << "mode " << iterStatuses->mode()
                                      << "transitions " << iterStatuses->transitions()
                                      << "duration " << iterStatuses->duration();
            }


            eligibilityMap.insert(std::pair<PublicKey, bool>(PublicKey(Slice(rep.crnpubkey().data(), rep.crnpubkey().size())), true));
        }
        JLOG(journal_.info()) << "TMDFSReportState::evaluateResponse() :::::::::::::::::::::::::::::::::::::::: VERBOSE PRINTEND ::::::::::::::::::::::::::::::::::::::::";

        app_.getCRNRound().updatePosition(eligibilityMap);
        // jrojek TODO: apply some formula to determine Yes/No eligibility for fee payout
        // jrojek TODO: and spread the news with app_
    }
}

void TMDFSReportState::evaluateAck(const std::shared_ptr<protocol::TMDFSReportStateAck> &m)
{
    JLOG(journal_.info()) << "TMDFSReportState::evaluateAck() " << m->dfsroot();
    overlay_.getDFSReportStateData().cancelTimer(m->dfsroot(), TMDFSReportStateData::ACK_TIMER);
}

} // namespace casinocoin

