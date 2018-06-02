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

namespace casinocoin {

class TMDFSReportState : public std::enable_shared_from_this<TMDFSReportState>

{
public:
    TMDFSReportState(Application& app,
                     OverlayImpl& overlay,
                     boost::asio::io_service& io_service,
                     PeerImp& parent,
                     beast::Journal journal);

    bool start();

    void evaluateRequest (std::shared_ptr <protocol::TMDFSReportState> const& m);
    void evaluateResponse (std::shared_ptr <protocol::TMDFSReportState> const& m);
    void evaluateAck (std::shared_ptr <protocol::TMDFSReportStateAck> const& m);

private:
    using error_code = boost::system::error_code;

    void reset();

    void setTimer(std::string const& pubKeyString);
    void cancelTimer(std::string const& pubKeyString);
    void onTimer (error_code ec);

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
    boost::asio::io_service::strand strand_;
    boost::asio::io_service& io_service_;
    std::map<std::string, std::unique_ptr<boost::asio::basic_waitable_timer<std::chrono::steady_clock>>> timers_;


};

} // namespace casinocoin

#endif // CASINOCOIN_OVERLAY_TMDFSReportState_H_INCLUDED
