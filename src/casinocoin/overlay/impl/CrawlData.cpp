#include "CrawlData.h"
#include <casinocoin/overlay/impl/OverlayImpl.h>

namespace casinocoin {

CrawlData::CrawlData()
    : ackTimer_(this)
    , responseTimer_(this)
    , overlay_(nullptr)
{}

CrawlData::CrawlData(OverlayImpl* overlay, beast::Journal journal)
    : ackTimer_(this)
    , responseTimer_(this)
    , overlay_(overlay)
    , journal_(journal)
{}

void CrawlData::conclude(bool forceConcluded)
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    state_ = forceConcluded ? FORCE_CONCLUDED : CONCLUDED;
}

bool CrawlData::concluded() const
{
    if (state_ != RUNNING)
    {
        if (state_ == CONCLUDED)
        {
            JLOG(journal_.debug()) << "Crawl concluded normally";
        }
        if (state_ == FORCE_CONCLUDED)
        {
            JLOG(journal_.debug()) << "Crawl force concluded";
        }
        return true;
    }
    return false;
}

void CrawlData::start()
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    lastReqRecipient_.clear();
    lastMsg_.Clear();
    ackTimer_.cancel();
    responseTimer_.cancel();
    state_ = RUNNING;
}

void CrawlData::setRecipient(const std::string &recipient)
{
    if (concluded())
        return;

    std::lock_guard<decltype(mutex_)> lock(mutex_);
    lastReqRecipient_ = recipient;
}

std::string const& CrawlData::getRecipient() const
{
    return lastReqRecipient_;
}

void CrawlData::setMsg(const protocol::TMDFSReportState &msg)
{
    if (concluded())
        return;

    std::lock_guard<decltype(mutex_)> lock(mutex_);
    lastMsg_ = msg;
}

protocol::TMDFSReportState const& CrawlData::getMsg() const
{
    return lastMsg_;
}

void CrawlData::startAckTimer(std::chrono::milliseconds timeout)
{
    if (concluded())
        return;

    std::lock_guard<decltype(mutex_)> lock(mutex_);

    JLOG(journal_.debug()) << "CrawlData::startAckTimer timeout ms: " << timeout.count();
    ackTimer_.setExpiration(timeout);
}

void CrawlData::cancelAckTimer()
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    ackTimer_.cancel();
}

void CrawlData::startResponseTimer(std::chrono::milliseconds timeout)
{
    if (concluded())
        return;

    std::lock_guard<decltype(mutex_)> lock(mutex_);

    JLOG(journal_.debug()) << "CrawlData::startResponseTimer timeout ms: " << timeout.count();
    responseTimer_.setExpiration(timeout);
}

void CrawlData::cancelResponseTimer()
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    responseTimer_.cancel();
}

bool CrawlData::isValid() const
{
    // jrojek: this is to ensure that instance was created on purpose, not via default construction
    return (overlay_ != nullptr);
}

void CrawlData::onDeadlineTimer(DeadlineTimer &timer)
{
    JLOG(journal_.debug()) << "CrawlData::onDeadlineTimer";
    protocol::TMDFSReportState msgToSend;
    std::string recipient;
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (timer == ackTimer_)
        {
            responseTimer_.cancel();
        }
        else if (timer == responseTimer_)
        {
        }
        else
        {
            return;
        }
        lastMsg_.add_visited(getRecipient());

        msgToSend = getMsg();
        recipient = getRecipient();
    }

    if (overlay_ != nullptr)
    {
        Overlay::PeerSequence knownPeers = overlay_->getSanePeers();
        if (knownPeers.size() > 0)
            // jrojek need to call that on any instance of TMDFSReportState as this is basically callback to 'me'
        {
            JLOG(journal_.debug()) << "CrawlData::onDeadlineTimer add node " << recipient << " as visitedS";
            knownPeers[0]->dfsReportState().addTimedOutNode(std::make_shared<protocol::TMDFSReportState>(msgToSend), recipient);
        }
    }
    else
        JLOG(journal_.info()) << "Overlay not initialised";
}

}
