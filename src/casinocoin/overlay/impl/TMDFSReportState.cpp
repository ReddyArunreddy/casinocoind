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

TMDFSReportState::TMDFSReportState(Application& app, OverlayImpl& overlay, beast::Journal journal)
    : app_(app)
    , overlay_(overlay)
    , journal_(journal)
    , pubKeyString_(toBase58(TOKEN_NODE_PUBLIC, app_.nodeIdentity().first))
{
    JLOG(journal_.info()) << "TMDFSReportState::TMDFSReportState() created for " << pubKeyString_;
}

bool TMDFSReportState::start()
{
    if (isStarted_)
    {
        JLOG(journal_.warn()) << "TMDFSReportState::start() is already started at " << start_;
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
}

void TMDFSReportState::onMessage(std::shared_ptr<protocol::TMDFSReportStateReq> const& m)
{
    JLOG(journal_.info()) << "TMDFSReportState::onMessage() TMDFSReportStateReq on " << pubKeyString_;

    if (m->visited().find(pubKeyString_) != m->visited().end())
    {
        JLOG(journal_.error()) << "TMDFSReportState::onMessage() TMDFSReportStateReq received Req in a node which is already on the list! " << pubKeyString_;
        return;
    }

    protocol::TMDFSReportStateReq forwardMsg = *m;
    // jrojek TODO: On request, need to add current node to the lists,
    //              check for first peer which is not already on the list
    //              and call request on that peer
    //              when all peers are on the list already, return Response to caller
    // open point: how to determine the caller? in a message! (last entry in dfs list)
}

void TMDFSReportState::onMessage(const std::shared_ptr<protocol::TMDFSReportStateResp> &m)
{
    // jrojek TODO: when response is received, need to check if there are still
    //              peers on our list who were not visited, and call request on them, if not
    //              call Response on caller
}

void TMDFSReportState::reset()
{
    reportState_.clear();
    visited_.clear();
    dfs_.clear();
}

} // namespace casinocoin

