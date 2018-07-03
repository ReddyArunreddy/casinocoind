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

void TMDFSReportStateData::startCrawl(CrawlInstance const& crawlInstance)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::startCrawl()"
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if (crawls_.find(crawlInstance) == crawls_.end())
        startCrawl_private(crawlInstance);
}

bool TMDFSReportStateData::exists(CrawlInstance const& crawlInstance) const
{
    return (crawls_.find(crawlInstance) != crawls_.end());
}

void TMDFSReportStateData::restartTimers(CrawlInstance const& crawlInstance,
                                        std::string const& currRecipient,
                                        protocol::TMDFSReportState const& currPayload)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::restartTimers()"
                          << " initiator: " << crawlInstance.initiator_
                          << " startLedger: " << crawlInstance.startLedger_
                          << " curr recipient: " << currRecipient;

    if (crawls_[crawlInstance]->concluded())
        return;

    std::unique_ptr<CrawlData>& theCrawl = crawls_[crawlInstance];
    theCrawl->startAckTimer(1s);
    theCrawl->startResponseTimer(50s / (currPayload.dfs_size() > 0 ? currPayload.dfs_size() : 1));
    theCrawl->setRecipient(currRecipient);
    theCrawl->setMsg(currPayload);

}

void TMDFSReportStateData::cancelTimer(CrawlInstance const& crawlInstance, CrawlData::TimerType type)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::cancelTimer() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;

    std::lock_guard<decltype(mutex_)> lock(mutex_);
    cancelTimer_private(crawlInstance, type);
}

protocol::TMDFSReportState const& TMDFSReportStateData::getLastRequest(CrawlInstance const& crawlInstance) const
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::getLastRequest() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;
    if (crawls_.find(crawlInstance) != crawls_.end())
    {
        return crawls_.at(crawlInstance)->getMsg();
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportStateData::getLastRequest couldn't find crawl data "
                              << " initiator: " << crawlInstance.initiator_
                              << " startLedger: " << crawlInstance.startLedger_;
    }
    return msgNone_;
}

std::string const& TMDFSReportStateData::getLastRecipient(CrawlInstance const& crawlInstance) const
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::getLastRecipient() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;
    if (crawls_.find(crawlInstance) != crawls_.end())
    {
        return crawls_.at(crawlInstance)->getRecipient();
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportStateData::getLastRecipient couldn't find crawl data "
                              << " initiator: " << crawlInstance.initiator_
                              << " startLedger: " << crawlInstance.startLedger_;
    }
    return recipientNone_;
}

void TMDFSReportStateData::conclude(CrawlInstance const&crawlInstance)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::conclude() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if (crawls_.find(crawlInstance) != crawls_.end())
    {
        crawls_[crawlInstance]->conclude(false);
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportStateData::conclude couldn't find crawl data "
                              << " initiator: " << crawlInstance.initiator_
                              << " startLedger: " << crawlInstance.startLedger_;
    }
}

bool TMDFSReportStateData::isConcluded(CrawlInstance const& crawlInstance) const
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::isConcluded() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;
    if (crawls_.find(crawlInstance) != crawls_.end())
    {
        return crawls_.at(crawlInstance)->concluded();
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportStateData::isConcluded couldn't find crawl data "
                              << " initiator: " << crawlInstance.initiator_
                              << " startLedger: " << crawlInstance.startLedger_;
    }
    return true;
}

void TMDFSReportStateData::startCrawl_private(CrawlInstance const& crawlInstance)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::startCrawl_private() crawls_ size before: " << crawls_.size();
    // jrojek FIXME (or at least const-me)
    if (static_cast<uint32_t>(crawlInstance.startLedger_) > (6*CRNPerformance::getReportingPeriod()))
    {
        CrawlInstance previousCrawl { crawlInstance.initiator_, (static_cast<uint32_t>(crawlInstance.startLedger_) - (5*CRNPerformance::getReportingPeriod()))};
        crawls_.erase(previousCrawl);
    }

    crawls_[crawlInstance].reset(new CrawlData(&overlay_, journal_));
    crawls_[crawlInstance]->start();
    JLOG(journal_.debug()) << "TMDFSReportStateData::startCrawl_private() crawls_ size after: " << crawls_.size();
}

void TMDFSReportStateData::cancelTimer_private(CrawlInstance const& crawlInstance, CrawlData::TimerType type)
{
    JLOG(journal_.debug()) << "TMDFSReportStateData::cancelTimer_private() "
                           << " initiator: " << crawlInstance.initiator_
                           << " startLedger: " << crawlInstance.startLedger_;
    if (crawls_.find(crawlInstance) != crawls_.end())
    {
        if (type == CrawlData::ACK_TIMER)
            crawls_[crawlInstance]->cancelAckTimer();
        else if (type == CrawlData::RESPONSE_TIMER)
            crawls_[crawlInstance]->cancelResponseTimer();
    }
    else
    {
        JLOG(journal_.warn()) << "TMDFSReportStateData::cancelTimer_private couldn't find crawl data "
                              << " initiator: " << crawlInstance.initiator_
                              << " startLedger: " << crawlInstance.startLedger_;
    }
}

}
