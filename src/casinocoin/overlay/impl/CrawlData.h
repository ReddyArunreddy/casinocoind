#ifndef CRAWLDATA_H
#define CRAWLDATA_H

#include <casinocoin/basics/Log.h>
#include <casinocoin/core/DeadlineTimer.h>
#include <casinocoin/overlay/impl/ProtocolMessage.h>
#include <casinocoin/app/misc/CRN.h>

namespace casinocoin {
class OverlayImpl;

class CrawlData : public DeadlineTimer::Listener
{
public:
    CrawlData();
    CrawlData(OverlayImpl* overlay, beast::Journal journal);

    enum TimerType
    { ACK_TIMER = 0
    , RESPONSE_TIMER = 1
    };

    enum State
    { RUNNING = 0           // normal state while collecting data
    , CONCLUDED = 1         // concluded normally and no operations can be accepted until next start call
    , FORCE_CONCLUDED = 2   // force concluded. crawl did not finish on time but had to report state before 'flag' ledger
    };

    void start();
    void conclude(CRN::EligibilityMap const& eligibilityMap, bool forceConcluded = false);
    bool concluded() const;

    CRN::EligibilityMap const& eligibilityMap() const;

    void setRecipient(std::string const& recipient);
    std::string const& getRecipient() const;
    void setMsg(protocol::TMDFSReportState const& msg);
    protocol::TMDFSReportState const& getMsg() const;

    void startAckTimer(std::chrono::milliseconds timeout);
    void cancelAckTimer();
    void startResponseTimer(std::chrono::milliseconds timeout);
    void cancelResponseTimer();

private:
    void onDeadlineTimer (DeadlineTimer& timer) override;
    std::string lastReqRecipient_;
    protocol::TMDFSReportState lastMsg_;
    DeadlineTimer ackTimer_;
    DeadlineTimer responseTimer_;
    State state_;
    CRN::EligibilityMap eligibilityMap_;

    std::mutex mutex_;
    OverlayImpl* overlay_;
    beast::Journal journal_;
};
}
#endif // CRAWLDATA_H
