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
        JLOG(journal_.debug()) << "Crawl " << state_ == CONCLUDED ? "concluded normally" : "force concluded";
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

    ackTimer_.setExpiration(timeout);
}

void CrawlData::cancelAckTimer()
{
    ackTimer_.cancel();
}

void CrawlData::startResponseTimer(std::chrono::milliseconds timeout)
{
    if (concluded())
        return;
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    responseTimer_.setExpiration(timeout);
}

void CrawlData::cancelResponseTimer()
{
    responseTimer_.cancel();
}

void CrawlData::onDeadlineTimer(DeadlineTimer &timer)
{
    protocol::TMDFSReportState msgToSend;
    std::string recipient;
    {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (timer == ackTimer_)
        {
            cancelResponseTimer();
        }
        else if (timer == responseTimer_)
        {
        }
        else
        {
            return;
        }
        setMsg(getMsg().add_visited(getRecipient()));

        msgToSend = getMsg();
        recipient = getRecipient();
    }

    if (overlay_ != nullptr)
    {
        Overlay::PeerSequence knownPeers = overlay_->getSanePeers();
        if (knownPeers.size() > 0)
            // jrojek need to call that on any instance of TMDFSReportState as this is basically callback to 'me'
        {
            knownPeers[0]->dfsReportState().addTimedOutNode(std::make_shared<protocol::TMDFSReportState>(msgToSend), recipient);
        }
    }
    else
        JLOG(journal_.info()) << "Overlay not initialised";
}

}
