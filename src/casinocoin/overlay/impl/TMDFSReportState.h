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

#ifndef CASINOCOIN_OVERLAY_TMDFSREPORTSTATE_H_INCLUDED
#define CASINOCOIN_OVERLAY_TMDFSREPORTSTATE_H_INCLUDED

#include <casinocoin/basics/Log.h>
#include <casinocoin/app/misc/CRN.h>
#include <casinocoin/protocol/JsonFields.h>
#include <casinocoin/protocol/tokens.h>
#include <casinocoin/protocol/digest.h>
#include <casinocoin/protocol/PublicKey.h>
#include <boost/algorithm/string/predicate.hpp>
#include <casinocoin/overlay/impl/ProtocolMessage.h>
#include <casinocoin/overlay/impl/OverlayImpl.h>
#include <casinocoin/core/DeadlineTimer.h>

namespace casinocoin {

// jrojek TODO: add separate timer to protect crawl break
// when some node respond with ACK but does not respond in sane time
// we should mark it as suspicious node and evaluate it accordingly in future

class TMDFSReportState
{
public:
    TMDFSReportState(Application& app,
                     OverlayImpl& overlay,
                     PeerImp& parent,
                     beast::Journal journal);
    ~TMDFSReportState();
    void start();

    void evaluateRequest (std::shared_ptr <protocol::TMDFSReportState> const& m);
    void evaluateResponse (std::shared_ptr <protocol::TMDFSReportState> const& m);
    void evaluateAck (std::shared_ptr <protocol::TMDFSReportStateAck> const& m);
    void addTimedOutNode(std::shared_ptr <protocol::TMDFSReportState> const& m, std::string const& timedOutNode);
    void forceConclude ();

private:

    void conclude (std::shared_ptr <protocol::TMDFSReportState> const& m, bool forceConclude);    
    void fillMessage (protocol::TMDFSReportState& m);
    bool forwardRequest (std::shared_ptr <protocol::TMDFSReportState> const& m);
    bool forwardResponse (std::shared_ptr <protocol::TMDFSReportState> const& m);
    bool checkReq (std::shared_ptr <protocol::TMDFSReportState> const& m);
    bool checkResp (std::shared_ptr <protocol::TMDFSReportState> const& m);

    Application& app_;
    OverlayImpl& overlay_;
    PeerImp& parentPeer_;
    beast::Journal journal_;

    std::string pubKeyString_;
    std::shared_ptr <protocol::TMDFSReportState> lastMessage_;
    bool crawlRunning_;
};

} // namespace casinocoin

#endif // CASINOCOIN_OVERLAY_TMDFSREPORTSTATE_H_INCLUDED
