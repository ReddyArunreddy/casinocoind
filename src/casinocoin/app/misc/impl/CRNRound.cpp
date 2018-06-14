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
#include <casinocoin/app/misc/FeeVote.h>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <algorithm>
#include <mutex>

namespace casinocoin {

/** The status of all nodes requested in a given period. */
struct NodesEligibilitySet
{
private:
    // How many yes/no votes each node received
    // kept in two distinct lists in case some sopthisticated logic needs to be applied
    hash_map<PublicKey, uint32> yesVotes_;
    hash_map<PublicKey, uint32> nayVotes_;
    CSCAmount feeDistributionVote_;
    CSCAmount feeRemainFromShare_;
    bool votingFinished_ = false;

    CRN::EligibilityPaymentMap paymentMap_;
public:
    using YesNayVotes = std::pair<uint32, uint32>;

    // number of trusted validations
    int mTrustedValidations = 0;

    // number of votes needed
    int mThreshold = 0;

    NodesEligibilitySet () = default;

    void tally (CRN::EligibilityMap const& nodes)
    {
        if (votingFinished_)
            return;

        ++mTrustedValidations;

        for (auto iter = nodes.begin(); iter != nodes.end() ; ++iter)
        {
            if (iter->second)
                ++yesVotes_[iter->first];
            else
                ++nayVotes_[iter->first];
        }
    }

    void setVotingFinished()
    {
        votingFinished_ = true;

        std::vector<PublicKey> eligibleList;
        for ( auto const& yesVote : yesVotes_)
        {
            if (isEligible(yesVote.first))
                eligibleList.push_back(yesVote.first);
        }
        if (eligibleList.size() == 0)
            return;

        feeRemainFromShare_ = CSCAmount(feeDistributionVote_.drops() % eligibleList.size());
        uint64_t sharePerNode = feeDistributionVote_.drops() / eligibleList.size();

        for ( auto const& eligibleNode : eligibleList)
            paymentMap_.insert(std::pair<PublicKey, CSCAmount>(eligibleNode, CSCAmount(sharePerNode)));
    }

    CRN::EligibilityPaymentMap votes () const
    {
        if (!votingFinished_)
            return CRN::EligibilityPaymentMap();

        return paymentMap_;
    }

    void setFeeDistributionVote(CSCAmount const& feeDistributionVote)
    {
        if (votingFinished_)
            return;

        feeDistributionVote_ = feeDistributionVote;
    }

    CSCAmount feeDistributionVote() const
    {
        return CSCAmount(feeDistributionVote_ - feeRemainFromShare_);
    }

private:
    bool isEligible(PublicKey const& crnNode) const
    {
        uint32_t votesCombined = 0;

        auto const& itYes = yesVotes_.find (crnNode);
        if (itYes != yesVotes_.end())
            votesCombined += itYes->second;

        auto const& itNay = nayVotes_.find (crnNode);
        if (itNay != nayVotes_.end())
            votesCombined -= itNay->second;

        return votesCombined >= mThreshold;
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
    std::unique_ptr <NodesEligibilitySet> lastVote_;
    CSCAmount lastFeeDistributionPosition_;

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
    JLOG (j_.info()) <<
        "CRNRoundImpl::doValidation with " << eligibilityMap_.size() << " candidates";

    std::lock_guard <std::mutex> sl (mutex_);

    STArray crnArray(sfCRNs);
    for ( auto iter = eligibilityMap_.begin(); iter != eligibilityMap_.end(); ++iter)
    {
        crnArray.push_back (STObject (sfCRN));
        auto& entry = crnArray.back ();
        entry.emplace_back (STBlob (sfCRN_PublicKey, iter->first.data(), iter->first.size()));
        entry.emplace_back (STUInt8 (sfCRNEligibility, iter->second ? 1 : 0));
    }

    lastFeeDistributionPosition_ = CSCAmount(SYSTEM_CURRENCY_START) - lastClosedLedger->info().drops;

    baseValidation.setFieldArray(sfCRNs, crnArray);
    baseValidation.setFieldAmount(sfCRN_FeeDistributed, STAmount(lastFeeDistributionPosition_));
}

void CRNRoundImpl::doVoting(std::shared_ptr<const ReadView> const& lastClosedLedger, const ValidationSet &parentValidations, const std::shared_ptr<SHAMap> &initialPosition)
{
    JLOG(j_.info()) << "CRNRoundImpl::doVoting. validations: " << parentValidations.size();

    detail::VotableInteger<std::int64_t> feeToDistribute (0, CSCAmount(SYSTEM_CURRENCY_START - lastClosedLedger->info().drops.drops()).drops());
    auto crnVote = std::make_unique<NodesEligibilitySet>();

    // based on other votes, conclude what in our POV elibigible nodes should look like
    for ( auto const& singleValidation : parentValidations)
    {
        if (!(singleValidation.second->isTrusted()))
            continue;
        CRN::EligibilityMap singleNodePosition;
        if (singleValidation.second->isFieldPresent(sfCRNs))
        {

            // get all votes for CRNs of given validator
            STArray const& crnVotesOfNode =
                    singleValidation.second->getFieldArray(sfCRNs);
            for ( auto voteOfNodeIter = crnVotesOfNode.begin(); voteOfNodeIter != crnVotesOfNode.end(); ++voteOfNodeIter)
            {
                STObject const& crnSTObject = *voteOfNodeIter;
                // *voteOfNodeIter is a single STObject containing CRN vote data
                if (crnSTObject.isFieldPresent(sfCRN_PublicKey) && crnSTObject.isFieldPresent(sfCRNEligibility))
                {
                    PublicKey crnPubKey(Slice(crnSTObject.getFieldVL(sfCRN_PublicKey).data(), crnSTObject.getFieldVL(sfCRN_PublicKey).size()));
                    bool crnEligibility = (crnSTObject.getFieldU8(sfCRNEligibility) > 0) ? true : false;

                    singleNodePosition.insert(
                                std::pair<PublicKey, bool>(crnPubKey, crnEligibility));
                }
            }
        }
        if (singleValidation.second->isFieldPresent(sfCRN_FeeDistributed))
        {
            feeToDistribute.addVote(singleValidation.second->getFieldAmount(sfCRN_FeeDistributed).csc().drops());
        }
        else
        {
            feeToDistribute.noVote();
        }
        crnVote->tally (singleNodePosition);
    }
    crnVote->mThreshold = std::max(1, (crnVote->mTrustedValidations * majorityFraction_) / 256);
    crnVote->setFeeDistributionVote(CSCAmount(feeToDistribute.getVotes()));
    crnVote->setVotingFinished();

    JLOG (j_.info()) <<
        "Received " << crnVote->mTrustedValidations <<
        " trusted validations, threshold is: " << crnVote->mThreshold;
    JLOG (j_.info()) <<
        " feeDistribution. our position: " << lastFeeDistributionPosition_.drops() <<
        " concluded vote: " << crnVote->feeDistributionVote().drops();


    {
        auto const seq = lastClosedLedger->info().seq + 1;
        STArray crnArray(sfCRNs);
        STAmount feeToDistributeST(crnVote->feeDistributionVote());

        CRN::EligibilityPaymentMap txVoteMap = crnVote->votes();
        if (txVoteMap.size() == 0)
        {
            JLOG(j_.warn()) << "No nodes eligible for payout. giving up this time";
            return;
        }

        for ( auto iter = txVoteMap.begin(); iter != txVoteMap.end(); ++iter)
        {
            crnArray.push_back (STObject (sfCRN));
            auto& entry = crnArray.back ();
            entry.emplace_back (STBlob (sfCRN_PublicKey, iter->first.data(), iter->first.size()));
            STAmount crnFeeDistributed(iter->second);
            crnFeeDistributed.setFName(sfCRN_FeeDistributed);
            entry.emplace_back (crnFeeDistributed);
        }

        JLOG(j_.warn()) << "We are voting for a CRNEligibility";

        STTx crnRoundTx (ttCRN_ROUND,
            [seq, crnArray, feeToDistributeST](auto& obj)
            {
                obj[sfAccount] = AccountID();
                obj[sfLedgerSequence] = seq;
                obj[sfCRN_FeeDistributed] = feeToDistributeST;
                obj.setFieldArray(sfCRNs, crnArray);
            });
        
        uint256 txID = crnRoundTx.getTransactionID ();
        
        JLOG(j_.warn()) << "CRNRound tx id: " << txID;
        
        Serializer s;
        crnRoundTx.add (s);
        
        auto tItem = std::make_shared<SHAMapItem> (txID, s.peekData ());
        
        if (!initialPosition->addGiveItem (tItem, true, false))
        {
            JLOG(j_.warn()) <<
                               "Ledger already had crn eligibility vote change";
        }
    }
    lastVote_ = std::move(crnVote);
}

void CRNRoundImpl::updatePosition( CRN::EligibilityMap const& currentPosition)
{
    // call from outside to update our position
    std::lock_guard <std::mutex> sl (mutex_);
    eligibilityMap_ = currentPosition;
    JLOG (j_.info()) <<
        "CRNRoundImpl::updatePosition with " << eligibilityMap_.size() << " candidates";
}



std::unique_ptr<CRNRound> make_CRNRound(int majorityFraction, beast::Journal journal)
{
    return std::make_unique<CRNRoundImpl> (majorityFraction, journal);
}


}  // casinocoin
