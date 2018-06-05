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
    2018-05-14  jrojek          Created
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/app/main/Application.h>
#include <casinocoin/app/misc/CRNRound.h>
#include <casinocoin/app/misc/Validations.h>
#include <casinocoin/core/DatabaseCon.h>
#include <casinocoin/core/ConfigSections.h>
#include <casinocoin/protocol/JsonFields.h>
#include <casinocoin/protocol/TxFlags.h>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <algorithm>
#include <mutex>

namespace casinocoin {

/** The status of all nodes requested in a given period. */
struct NodesEligibilityState
{
private:
    // How many yes/no votes each node received
    hash_map<PublicKey, uint32> yesVotes_;
    hash_map<PublicKey, uint32> nayVotes_;

public:
    using YesNayVotes = std::pair<uint32, uint32>;

    // number of trusted validations
    int mTrustedValidations = 0;

    // number of votes needed
    int mThreshold = 0;

    NodesEligibilityState () = default;

    void tally (CRN::EligibilityMap const& nodes)
    {
        ++mTrustedValidations;

        for (auto iter = nodes.begin(); iter != nodes.end() ; ++iter)
        {
            if (iter->second)
                ++yesVotes_[iter->first];
            else
                ++nayVotes_[iter->first];
        }
    }

    YesNayVotes votes (PublicKey const& crnNode) const
    {
        YesNayVotes ret{0,0};

        auto const& itYes = yesVotes_.find (crnNode);
        if (itYes != yesVotes_.end())
            ret.first = itYes->second;

        auto const& itNay = nayVotes_.find (crnNode);
        if (itNay != nayVotes_.end())
            ret.second = itNay->second;

        return ret;
    }
};

class CRNRoundImpl final : public CRNRound
{
public:
    CRNRoundImpl (
        int majorityFraction,
        beast::Journal journal);

    void
    doValidation (std::shared_ptr<ReadView const> const& lastClosedLedger,
        STObject& baseValidation) override;

    void
    doVoting (std::shared_ptr<ReadView const> const& lastClosedLedger,
        ValidationSet const& parentValidations,
        std::shared_ptr<SHAMap> const& initialPosition) override;

    void
    updatePosition(CRN::EligibilityMap const& currentPosition) override;

protected:
    std::mutex mutex_;

    CRN::EligibilityMap eligibilityMap_;

    // The amount of support that an amendment must receive
    // 0 = 0% and 256 = 100%
    int const majorityFraction_;

    // The results of the last voting round - may be empty if
    // we haven't participated in one yet.
    std::unique_ptr <NodesEligibilityState> lastVote_;

    beast::Journal j_;

};

CRNRoundImpl::CRNRoundImpl(int majorityFraction, beast::Journal journal)
    : majorityFraction_(majorityFraction)
    , j_(journal)
{
    assert (majorityFraction_ != 0);
}

void CRNRoundImpl::doValidation(std::shared_ptr<const ReadView> const& lastClosedLedger, STObject &baseValidation)
{
    // update validation object with our position
}

void CRNRoundImpl::doVoting(std::shared_ptr<const ReadView> const& lastClosedLedger, const ValidationSet &parentValidations, const std::shared_ptr<SHAMap> &initialPosition)
{
    // calculate votes of other 'STValidation' propositions, add Tx to ledger.
}

void CRNRoundImpl::updatePosition( CRN::EligibilityMap const& currentPosition)
{
    // call from outside to update our position
    std::lock_guard <std::mutex> sl (mutex_);
    eligibilityMap_ = currentPosition;
}



std::unique_ptr<CRNRound> make_CRNRound(int majorityFraction, beast::Journal journal)
{
    return std::make_unique<CRNRoundImpl> (majorityFraction, journal);
}


}  // casinocoin
