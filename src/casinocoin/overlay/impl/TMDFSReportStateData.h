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

#ifndef CASINOCOIN_OVERLAY_TMDFSREPORTSTATEDATA_H_INCLUDED
#define CASINOCOIN_OVERLAY_TMDFSREPORTSTATEDATA_H_INCLUDED

#include <casinocoin/basics/Log.h>
#include <casinocoin/app/misc/CRN.h>
#include <casinocoin/protocol/JsonFields.h>
#include <casinocoin/protocol/tokens.h>
#include <casinocoin/protocol/digest.h>
#include <casinocoin/protocol/PublicKey.h>
#include <boost/algorithm/string/predicate.hpp>
#include <casinocoin/overlay/impl/ProtocolMessage.h>
#include <casinocoin/overlay/impl/CrawlData.h>

namespace casinocoin {
class OverlayImpl;

class TMDFSReportStateData
{
public:
    struct CrawlInstance
    {
        std::string initiator_;
        LedgerIndex startLedger_;

        bool operator==(CrawlInstance const& other) const
        {
            bool ret = true;
            ret &= initiator_ == other.initiator_;
            ret &= startLedger_ == other.startLedger_;
            return ret;
        }
    };

    TMDFSReportStateData(OverlayImpl& overlay,
                         beast::Journal journal);

    void startCrawl(CrawlInstance const& crawlInstance);
    bool exists(CrawlInstance const& crawlInstance) const;

    void restartTimers(CrawlInstance const& crawlInstance,
                      std::string const& currRecipient,
                      protocol::TMDFSReportState const& currPayload);

    void cancelTimer(CrawlInstance const& crawlInstance, CrawlData::TimerType type);

    protocol::TMDFSReportState const& getLastRequest(CrawlInstance const& crawlInstance) const;
    std::string const& getLastRecipient(CrawlInstance const& crawlInstance) const;

    void conclude(CrawlInstance const& crawlInstance);
    bool isConcluded(CrawlInstance const& crawlInstance) const;

private:
    void startCrawl_private(const CrawlInstance &crawlInstance);
    void cancelTimer_private(CrawlInstance const& crawlInstance, CrawlData::TimerType type);

    // jrojek: map contain base58 public key of initiator
    // (first entry on dfs list of TMDFSReportState) and a corresponding crawl instance data
    std::map<CrawlInstance, std::unique_ptr<CrawlData>> crawls_;

    std::mutex mutex_;

    OverlayImpl& overlay_;
    beast::Journal journal_;
};

} // namespace casinocoin

#endif // CASINOCOIN_OVERLAY_TMDFSREPORTSTATEDATA_H_INCLUDED

