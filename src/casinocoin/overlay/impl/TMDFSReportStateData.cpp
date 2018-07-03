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

#include "TMDFSReportStateData.h"
#include <casinocoin/overlay/impl/OverlayImpl.h>

namespace casinocoin {

TMDFSReportStateData::TMDFSReportStateData(OverlayImpl& overlay,
                                           beast::Journal journal)
    : overlay_(overlay)
    , journal_(journal)
{
}

void TMDFSReportStateData::startCrawl(const std::string &initiatorPubKey)
{
    crawls_[initiatorPubKey].reset(&overlay_, journal_);
}

void TMDFSReportStateData::restartTimers(std::string const& initiatorPubKey,
                                        std::string const& currRecipient,
                                        protocol::TMDFSReportState const& currPayload)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::restartACKTimer()"
                          << "initiator: " << initiatorPubKey
                          << " curr recipient: " << currRecipient;
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if (crawls_[initiatorPubKey]->concluded())
        return;

    unique_ptr<CrawlData> theCrawl = crawls_[initiatorPubKey];
    theCrawl->startAckTimer(1s);
    theCrawl->startResponseTimer(20s);
    theCrawl->setRecipient(currRecipient);
    theCrawl->setMsg(currPayload);
}

void TMDFSReportStateData::cancelTimer(std::string const& initiatorPubKey, CrawlData::TimerType type)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::cancelTimer() "
                          << "initiator: " << initiatorPubKey;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    cancelTimer_private(initiatorPubKey, type);
}

protocol::TMDFSReportState const& TMDFSReportStateData::getLastRequest(std::string const& initiatorPubKey) const
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::getLastRequest() "
                          << "initiator: " << initiatorPubKey;
    return crawls_.at(initiatorPubKey)->getMsg();
}

std::string const& TMDFSReportStateData::getLastRecipient(std::string const& initiatorPubKey) const
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::getLastRecipient() "
                          << "initiator: " << initiatorPubKey;
    return crawls_.at(initiatorPubKey)->getRecipient();
}

void TMDFSReportStateData::conclude(const std::string &initiatorPubKey)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::conclude() "
                          << "initiator: " << initiatorPubKey;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    crawls_[initiatorPubKey]->conclude(false);
}

void TMDFSReportStateData::cancelTimer_private(const std::string &initiatorPubKey, CrawlData::TimerType type)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::cancelTimer_private() "
                          << "initiator: " << initiatorPubKey;
    if (crawls_.find(initiatorPubKey) != crawls_.end())
    {
        if (type == CrawlData::ACK_TIMER)
            crawls_[initiatorPubKey]->cancelAckTimer();
        else if (type == CrawlData::RESPONSE_TIMER)
            crawls_[initiatorPubKey]->cancelResponseTimer();
    }
    else
        JLOG(journal_.warn()) << "TMDFSReportStateData::cancelTimer_private couldn't find crawl data "
                              << "for root node: " << initiatorPubKey;
}

}
