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
#include <casinocoin/app/misc/CRNList.h>

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

void TMDFSReportState::start(LedgerIndex const& startLedger)
{
    JLOG(journal_.debug()) << "TMDFSReportState::start TMDFSReportState";
    protocol::TMDFSReportState msg;

    fillMessage(msg);
    msg.set_startledger(static_cast<uint32_t>(startLedger));
    msg.set_type(protocol::TMDFSReportState::rtREQ);

    TMDFSReportStateData::CrawlInstance crawlInstance {pubKeyString_, startLedger};

    overlay_.getDFSReportStateData().startCrawl(crawlInstance);
    overlay_.getDFSReportStateData().restartTimers(crawlInstance,
                                                  toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic()),
                                                  msg);

    parentPeer_.send(std::make_shared<Message>(msg, protocol::mtDFS_REPORT_STATE));
}

void TMDFSReportState::evaluateRequest(std::shared_ptr<protocol::TMDFSReportState> const& m)
{
    JLOG(journal_.debug()) << "TMDFSReportState::evaluateRequest() node "
                          << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic())
                          << " dfsSize " << m->dfs_size();
    for (std::string const& dfsEntry : m->dfs())
        JLOG(journal_.debug()) << "dfs: " << dfsEntry;

    if (shouldForceConclude(m))
    {
        conclude(m, true);
        return;
    }

    if (!checkReq(m))
        return;

    fillMessage(*m);

    m->set_type(protocol::TMDFSReportState::rtREQ);

    if (forwardRequest(m))
        return;

    // if we reach this point this means that we already visited all our peers and we know of their state
    m->set_type(protocol::TMDFSReportState::rtRESP);
    parentPeer_.send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));
}

void TMDFSReportState::evaluateResponse(std::shared_ptr<protocol::TMDFSReportState> const&m)
{
    JLOG(journal_.debug()) << "TMDFSReportState::evaluateResponse node "
                          << toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic())
                          << " dfsSize " << m->dfs_size();
    for (std::string const& dfsEntry : m->dfs())
        JLOG(journal_.debug()) << "dfs: " << dfsEntry;

    if (shouldForceConclude(m))
    {
        conclude(m, true);
        return;
    }

    if (!checkResp(m))
        return;

    m->set_type(protocol::TMDFSReportState::rtREQ);

    if (forwardRequest(m))
        return;

    m->set_type(protocol::TMDFSReportState::rtRESP);

    if (forwardResponse(m))
        return;

    // jrojek: reaching this point means we are the initiator of crawl and should now conclude
    conclude(m);
}

void TMDFSReportState::evaluateAck(const std::shared_ptr<protocol::TMDFSReportStateAck> &m)
{
    if (!m->has_startledger())
    {
        JLOG(journal_.error()) << "TMDFSReportState::evaluateAck() old protocol version. Please update to most recent one";
        return;
    }
    TMDFSReportStateData::CrawlInstance crawlInstance = {m->dfsroot(), m->startledger()};

    JLOG(journal_.debug()) << "TMDFSReportState::evaluateAck() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;
    overlay_.getDFSReportStateData().cancelTimer(crawlInstance, CrawlData::ACK_TIMER);
}

void TMDFSReportState::addTimedOutNode(std::shared_ptr<protocol::TMDFSReportState> const& m, const std::string &timedOutNode)
{
    JLOG(journal_.debug()) << "TMDFSReportState::addTimedOutNode() " << timedOutNode
                          << " dfsSize " << m->dfs_size();
    for (std::string const& dfsEntry : m->dfs())
        JLOG(journal_.debug()) << "dfs: " << dfsEntry;

    if (shouldForceConclude(m))
    {
        conclude(m, true);
        return;
    }

    m->set_type(protocol::TMDFSReportState::rtREQ);

    if (forwardRequest(m))
        return;

    m->set_type(protocol::TMDFSReportState::rtRESP);

    if (forwardResponse(m))
        return;

    // jrojek: reaching this point means we are the initiator of crawl and should now conclude
    conclude(m);
}

bool TMDFSReportState::shouldForceConclude(std::shared_ptr<protocol::TMDFSReportState> const& m) const
{
    if (m->dfs_size() == 0)
    {
        JLOG(journal_.debug()) << "TMDFSReportState::shouldForceConclude() dfs list already empty. return true";
        return true;
    }
    TMDFSReportStateData::CrawlInstance crawlInstance = {m->dfs(0), m->startledger()};
    JLOG(journal_.debug()) << "TMDFSReportState::shouldForceConclude() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;

    if (((app_.getLedgerMaster().getValidLedgerIndex() + 1) % CRNPerformance::getReportingPeriod()) == 0
            && !overlay_.getDFSReportStateData().isConcluded(crawlInstance))
    {
        return true;
    }
    return false;
}

void TMDFSReportState::conclude(std::shared_ptr<protocol::TMDFSReportState> const&m, bool forceConclude)
{
    if (!m->has_startledger())
    {
        JLOG(journal_.error()) << "TMDFSReportState::conclude() old protocol version. Please update to most recent one";
        return;
    }

    TMDFSReportStateData::CrawlInstance crawlInstance = {m->dfs(0), m->startledger()};
    if (m->dfs_size() != 0)
    {
        JLOG(journal_.warn()) << "TMDFSReportState::conclude() but dfs list is not empty...";
        for (std::string const& dfsEntry : m->dfs())
            JLOG(journal_.debug()) << "dfs: " << dfsEntry;
    }
    JLOG(journal_.info()) << "TMDFSReportState::conclude() Crawl for " << crawlInstance.initiator_ << " started at ledger: " << crawlInstance.startLedger_ << " concluded.";
    JLOG(journal_.info()) << "DFS list empty. final stats: visited: " << m->visited_size() << " CRN nodes reported: " << m->reports_size();
    JLOG(journal_.debug()) << "TMDFSReportState::conclude() :::::::::::::::::::::::: VERBOSE PRINTOUT :::::::::::::::::::::::";
    for (int i = 0; i < m->visited_size(); ++i)
            JLOG(journal_.debug()) << "TMDFSReportState::conclude() visited: " << m->visited(i);

    CRN::EligibilityMap eligibilityMap;
    for (auto iter = m->reports().begin() ; iter != m->reports().end() ; ++iter)
    {
        protocol::TMReportState const& rep = iter->report();
        if (rep.has_activated() && rep.has_crnpubkey() && rep.has_currstatus() && rep.has_domain() && rep.has_latency() && rep.has_ledgerseqbegin() && rep.has_ledgerseqend())
        {
            boost::optional<PublicKey> pk = PublicKey(Slice(rep.crnpubkey().data(), rep.crnpubkey().size()));
            JLOG(journal_.debug()) << "TMDFSReportState - currStatus " << rep.currstatus()
                                  << " ledgerSeqBegin " << rep.ledgerseqbegin()
                                  << " ledgerSeqEnd " << rep.ledgerseqend()
                                  << " latency " << rep.latency()
                                  << " crnPubKey " << toBase58(TOKEN_NODE_PUBLIC,*pk)
                                  << " domain " << rep.domain()
                                  << " signature " << rep.signature();
            for (auto iterStatuses = rep.status().begin() ; iterStatuses != rep.status().end() ; ++iterStatuses)
            {
                JLOG(journal_.debug()) << "mode " << iterStatuses->mode()
                                      << "transitions " << iterStatuses->transitions()
                                      << "duration " << iterStatuses->duration();
            }
            bool eligible = true;
            // check if node is on CRNList
            if(app_.relaynodes().listed(*pk))
            {
                // check if signature is valid
                auto unHexedSignature = strUnHex(rep.signature());
                if (unHexedSignature.second && pk)
                {
                    eligible &= casinocoin::verify(
                        *pk,
                        makeSlice(strHex(rep.domain())),
                        makeSlice(unHexedSignature.first)
                    );
                }
                else
                {
                    eligible &= false;
                    JLOG(journal_.debug()) << "TMDFSReportState - failed to read PubKey or signature of CRN candidate";
                }
                if(eligible)
                {
                    // check if account is funded
                    if (!CRNId::activated(*pk, app_.getLedgerMaster(), journal_, app_.config()))
                    {
                        JLOG(journal_.debug()) << "TMDFSReportState - Latency to high: " << toBase58(TOKEN_NODE_PUBLIC,*pk);
                        eligible &= false;
                    }
                    // check if latency is acceptable
                    if(rep.latency() > app_.config().CRN_MAX_LATENCY)
                    {
                        JLOG(journal_.debug()) << "TMDFSReportState - Latency to high: " << toBase58(TOKEN_NODE_PUBLIC,*pk);
                        eligible &= false;
                    }
                }
                else
                {
                    JLOG(journal_.debug()) << "TMDFSReportState - Signature is invalid: " << toBase58(TOKEN_NODE_PUBLIC,*pk);
                }
            }
            else
            {
                JLOG(journal_.debug()) << "TMDFSReportState - PublicKey not in CRNList: " << toBase58(TOKEN_NODE_PUBLIC,*pk);
                eligible &= false;
            }
            JLOG(journal_.info()) << "TMDFSReportState - PublicKey: " << toBase58(TOKEN_NODE_PUBLIC,*pk) << " Eligible:" << eligible;
            eligibilityMap.insert(std::pair<PublicKey, bool>(PublicKey(Slice(rep.crnpubkey().data(), rep.crnpubkey().size())), eligible));
        }
    }
    JLOG(journal_.debug()) << "TMDFSReportState::conclude() :::::::::::::: VERBOSE PRINTEND ::::::::::::::::::";

    app_.getCRNRound().updatePosition(eligibilityMap);
    overlay_.getDFSReportStateData().conclude(crawlInstance, forceConclude);
}

void TMDFSReportState::fillMessage(protocol::TMDFSReportState& m)
{
    JLOG(journal_.debug()) << "TMDFSReportState::fillMessage()"
                          << " dfsSize " << m.dfs_size();
    if (app_.isCRN())
    {
        protocol::TMDFSReportState::PubKeyReportMap* newEntry = m.add_reports ();
        newEntry->set_pubkey(pubKeyString_);
        newEntry->set_allocated_report( new protocol::TMReportState(app_.getCRN().performance().getPreparedReport()));
    }
    m.add_visited(pubKeyString_);
    m.add_dfs(pubKeyString_);
}

bool TMDFSReportState::forwardRequest(std::shared_ptr<protocol::TMDFSReportState> const&m)
{
    JLOG(journal_.debug()) << "TMDFSReportState::forwardRequest()"
                          << " dfsSize " << m->dfs_size();
    for (std::string const& dfsEntry : m->dfs())
        JLOG(journal_.debug()) << "dfs: " << dfsEntry;

    if (!m->has_startledger())
    {
        JLOG(journal_.error()) << "TMDFSReportState::forwardRequest() old protocol version. Please update to most recent one";
        return false;
    }

    Overlay::PeerSequence sanePeers = overlay_.getSanePeers();
    if (sanePeers.size() > 0)
    {
        for (auto const& singlePeer : sanePeers)
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

            TMDFSReportStateData::CrawlInstance crawlInstance = {m->dfs(0), m->startledger()};
            overlay_.getDFSReportStateData().restartTimers(crawlInstance,
                                                          toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()),
                                                          *m);

            singlePeer->send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));

            return true;
        }
    }
    else
    {
        JLOG(journal_.error()) << "TMDFSReportState::forwardRequest() "
                               << "Something went terribly wrong, no active peers discovered";
        return false;
    }
    return false;
}

bool TMDFSReportState::forwardResponse(const std::shared_ptr<protocol::TMDFSReportState> &m)
{
    JLOG(journal_.debug()) << "TMDFSReportState::forwardResponse()"
                          << " dfsSize " << m->dfs_size();
    for (std::string const& dfsEntry : m->dfs())
        JLOG(journal_.debug()) << "dfs: " << dfsEntry;

    if (!m->has_startledger())
    {
        JLOG(journal_.error()) << "TMDFSReportState::forwardResponse() old protocol version. Please update to most recent one";
        return false;
    }

    TMDFSReportStateData::CrawlInstance crawlInstance = {m->dfs(0), m->startledger()};
    auto dfsList = m->mutable_dfs();
    if (dfsList->size() > 0 && dfsList->Get(dfsList->size() - 1) == pubKeyString_)
    {
        overlay_.getDFSReportStateData().cancelTimer(crawlInstance, CrawlData::RESPONSE_TIMER);
        dfsList->RemoveLast();
    }
    else
    {
        JLOG(journal_.error()) << "TMDFSReportState::forwardResponse() couldn't remove 'me' "
                               << pubKeyString_ << " from DFS list";
        for (std::string const& dfsEntry : m->dfs())
            JLOG(journal_.debug()) << "dfs: " << dfsEntry;
        // jrojek: FIXME continue... it is basically sick behavior, but lets allow it for now
        // return false;
    }

    Overlay::PeerSequence sanePeers = overlay_.getSanePeers();
    if (dfsList->size() > 0)
    {
        for (auto const& singlePeer : sanePeers)
        {
            // jrojek: respond to sender
            if (toBase58(TOKEN_NODE_PUBLIC, singlePeer->getNodePublic()) == dfsList->Get(dfsList->size() - 1))
            {
                singlePeer->send(std::make_shared<Message>(*m, protocol::mtDFS_REPORT_STATE));
                return true;
            }
        }
    }
    return false;
}

bool TMDFSReportState::checkReq(std::shared_ptr<protocol::TMDFSReportState> const&m)
{
    JLOG(journal_.debug()) << "TMDFSReportState::checkReq()"
                          << " dfsSize " << m->dfs_size();
    for (std::string const& dfsEntry : m->dfs())
        JLOG(journal_.debug()) << "dfs: " << dfsEntry;

    if (m->type() != protocol::TMDFSReportState::rtREQ)
    {
        JLOG(journal_.error()) << "TMDFSReportState::checkReq() "
                               << "TMDFSReportState evaluating req but it is not a req";
        // jrojek... need to protect this situation not to break whole crawl procedure somehow
        // jrojek... for now just retrieve 'valid state'
        m->set_type(protocol::TMDFSReportState::rtREQ);
    }

    if (!m->has_startledger())
    {
        JLOG(journal_.error()) << "TMDFSReportState::checkReq() old protocol version. Please update to most recent one";
        return false;
    }

    for (std::string const& visitedNode : m->visited())
    {
        if (visitedNode == pubKeyString_)
        {
            JLOG(journal_.error()) << "TMDFSReportState::checkReq() "
                                   << "TMDFSReportState received Req in a node which is already on the list! " << pubKeyString_;
            return false;
        }
    }
    if (m->dfs_size() > 0)
    {
        TMDFSReportStateData::CrawlInstance crawlInstance = {m->dfs(0), m->startledger()};
        if (!overlay_.getDFSReportStateData().exists(crawlInstance))
        {
            overlay_.getDFSReportStateData().startCrawl(crawlInstance);
        }
        if (overlay_.getDFSReportStateData().isConcluded(crawlInstance))
        {
            JLOG(journal_.debug()) << "TMDFSReportState::checkReq() "
                                   << "crawl for " << crawlInstance.initiator_ << " at ledger " << crawlInstance.startLedger_ << " already concluded. Discarding msg";
            return false;
        }
    }
    return true;
}

bool TMDFSReportState::checkResp(std::shared_ptr<protocol::TMDFSReportState> const&m)
{
    JLOG(journal_.debug()) << "TMDFSReportState::checkResp()"
                          << " dfsSize " << m->dfs_size();
    for (std::string const& dfsEntry : m->dfs())
        JLOG(journal_.debug()) << "dfs: " << dfsEntry;

    if (m->type() != protocol::TMDFSReportState::rtRESP)
    {
        JLOG(journal_.error()) << "TMDFSReportState::checkResp() TMDFSReportState evaluating resp but it is not a resp";
        // jrojek... need to protect this situation not to break whole crawl procedure somehow
        // jrojek... for now just retrieve 'valid state'
        m->set_type(protocol::TMDFSReportState::rtRESP);
    }

    if (!m->has_startledger())
    {
        JLOG(journal_.error()) << "TMDFSReportState::checkResp() old protocol version. Please update to most recent one";
        return false;
    }

    // check if the response we recently received does not come from a node which already timed-out in our scope
    if (m->dfs_size() > 0)
    {
        TMDFSReportStateData::CrawlInstance crawlInstance = {m->dfs(0), m->startledger()};
        std::string parentPeerPubKey = toBase58(TOKEN_NODE_PUBLIC, parentPeer_.getNodePublic());
        auto const& visitedNodes = overlay_.getDFSReportStateData().getLastRequest(crawlInstance).visited();
        for (std::string const& visitedNode : visitedNodes)
        {
            if (visitedNode == parentPeerPubKey)
            {
                JLOG(journal_.warn()) << "TMDFSReportState::checkResp() received response from already visited peer: " << parentPeerPubKey
                                      << " Probably obsolete response";
                return false;
            }
        }

        if (overlay_.getDFSReportStateData().isConcluded(crawlInstance))
        {
            JLOG(journal_.debug()) << "TMDFSReportState::checkResp() "
                                   << "crawl for " << crawlInstance.initiator_ << " at ledger " << crawlInstance.startLedger_ << " already concluded. Discarding msg";
            return false;
        }
    }
    return true;
}

} // namespace casinocoin

