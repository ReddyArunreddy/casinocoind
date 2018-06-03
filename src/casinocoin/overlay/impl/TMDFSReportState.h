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

#ifndef CASINOCOIN_OVERLAY_TMDFSReportState_H_INCLUDED
#define CASINOCOIN_OVERLAY_TMDFSReportState_H_INCLUDED

#include <casinocoin/basics/Log.h>
#include <casinocoin/app/misc/CRN.h>
#include <casinocoin/protocol/JsonFields.h>
#include <casinocoin/protocol/tokens.h>
#include <casinocoin/protocol/digest.h>
#include <casinocoin/protocol/PublicKey.h>
#include <boost/algorithm/string/predicate.hpp>
#include <casinocoin/overlay/predicates.h>
#include <casinocoin/overlay/impl/ProtocolMessage.h>
#include <casinocoin/overlay/impl/OverlayImpl.h>
#include <casinocoin/core/DeadlineTimer.h>

namespace casinocoin {

class TMDFSReportState : public DeadlineTimer::Listener
{
public:
    TMDFSReportState(Application& app,
                     OverlayImpl& overlay,
                     PeerImp& parent,
                     beast::Journal journal);

    bool start();

    void evaluateRequest (std::shared_ptr <protocol::TMDFSReportState> const& m);
    void evaluateResponse (std::shared_ptr <protocol::TMDFSReportState> const& m);
    void evaluateAck (std::shared_ptr <protocol::TMDFSReportStateAck> const& m);

private:
    void onDeadlineTimer (DeadlineTimer& timer) override;

    std::chrono::system_clock::time_point start_;
    Application& app_;
    OverlayImpl& overlay_;
    PeerImp& parentPeer_;
    beast::Journal journal_;

    std::string pubKeyString_;
    bool isStarted_ = false;

    std::map<std::string, protocol::TMReportState> reportState_;
    std::map<std::string, bool> visited_;
    std::deque<std::string> dfs_;

    std::string lastReqRecipient_;
    protocol::TMDFSReportState lastReq_;


};

} // namespace casinocoin

#endif // CASINOCOIN_OVERLAY_TMDFSReportState_H_INCLUDED
