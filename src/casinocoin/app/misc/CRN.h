//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/casinocoin/casinocoind
    Copyright (c) 2018 CasinoCoin Foundation

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
    2018-05-26  jrojek          Created
*/
//==============================================================================

#ifndef CASINOCOIN_PROTOCOL_CRN_H
#define CASINOCOIN_PROTOCOL_CRN_H

#include <casinocoin/beast/utility/Journal.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/app/misc/CRNPerformance.h>
#include <casinocoin/app/misc/CRNId.h>
#include <casinocoin/overlay/impl/ProtocolMessage.h>

namespace casinocoin {

class CRN
{
public:
    CRN(PublicKey const& pubKey,
        std::string const& domain,
        std::string const& domainSignature,
        NetworkOPs& networkOps,
        LedgerIndex const& startupSeq,
        beast::Journal j);

    CRN(Section const& relaynodeConfig,
        NetworkOPs& networkOps,
        LedgerIndex const& startupSeq,
        beast::Journal j);

    bool onOverlayMessage(std::shared_ptr<protocol::TMReportState> const& m);

    Json::Value json() const;

    CRNId const& id() const;

    CRNPerformance& performance() const;

private:
    CRNId id_;
    std::unique_ptr<CRNPerformance> performance_;
};

std::unique_ptr<CRN> make_CRN(Section const& relaynodeConfig,
                              NetworkOPs& networkOps,
                              LedgerIndex const& startupSeq,
                              beast::Journal j);

std::unique_ptr<CRN> make_CRN(PublicKey const& pubKey,
                              std::string const& domain,
                              std::string const& domainSignature,
                              NetworkOPs& networkOps,
                              LedgerIndex const& startupSeq,
                              beast::Journal j);

}
#endif // CASINOCOIN_PROTOCOL_CRN_H

