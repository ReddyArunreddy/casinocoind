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

/** Current state of CRN in terms of eligibility for payout.
    Tells if node is qualified for payout
*/
struct CRNState
{
    /** Indicates an amendment that this server has code support for. */
    bool supported = false;

    /** The name of this amendment, possibly empty. */
    std::string name;

    CRNState () = default;
};

/** The status of all CRN eligibility requested in a given window. */
struct CRNSet
{
private:
    // How many yes votes each amendment received
    hash_map<uint256, int> votes_;

public:
    // number of trusted validations
    int mTrustedValidations = 0;

    // number of votes needed
    int mThreshold = 0;

    CRNSet () = default;

    void tally (std::set<uint256> const& amendments)
    {
        ++mTrustedValidations;

        for (auto const& amendment : amendments)
            ++votes_[amendment];
    }

    int votes (uint256 const& amendment) const
    {
        auto const& it = votes_.find (amendment);

        if (it == votes_.end())
            return 0;

        return it->second;
    }
};

//------------------------------------------------------------------------------

/** Track the list of "amendments"

   An "amendment" is an option that can affect transaction processing rules.
   Amendments are proposed and then adopted or rejected by the network. An
   Amendment is uniquely identified by its AmendmentID, a 256-bit key.
*/
class CRNRoundImpl final
    : public CRNRound
{
protected:
    std::mutex mutex_;

    hash_map<NodeID, CRNState> crnMap_;
    std::uint32_t lastUpdateSeq_;

    // The results of the last voting round - may be empty if
    // we haven't participated in one yet.
    std::unique_ptr <CRNSet> lastVote_;

    beast::Journal j_;

    // Finds or creates state
    CRNState* add (uint256 const& amendment);

    // Finds existing state
    CRNState* get (uint256 const& amendment);

    void setJson (Json::Value& v, uint256 const& amendment, const CRNState&);

public:
    CRNRoundImpl (
        std::chrono::seconds majorityTime,
        int majorityFraction,
        Section const& supported,
        Section const& enabled,
        Section const& vetoed,
        beast::Journal journal);

    uint256 find (std::string const& name) override;

    bool veto (uint256 const& amendment) override;
    bool unVeto (uint256 const& amendment) override;

    bool enable (uint256 const& amendment) override;
    bool disable (uint256 const& amendment) override;

    bool isEnabled (uint256 const& amendment) override;
    bool isSupported (uint256 const& amendment) override;

    Json::Value getJson (int) override;
    Json::Value getJson (uint256 const&) override;

    bool needValidatedLedger (LedgerIndex seq) override;

    void doValidatedLedger (
        LedgerIndex seq,
        std::set<uint256> const& enabled) override;

    std::vector <uint256>
    doValidation (std::set<uint256> const& enabledAmendments) override;

    std::vector <uint256>
    getDesired () override;

    std::map <uint256, std::uint32_t>
    doVoting (
        NetClock::time_point closeTime,
        std::set<uint256> const& enabledAmendments,
        majorityAmendments_t const& majorityAmendments,
        ValidationSet const& validations) override;
};

//------------------------------------------------------------------------------

CRNRoundImpl::CRNRoundImpl (
        Section const& supported,
        Section const& enabled,
        Section const& vetoed,
        beast::Journal journal)
    : lastUpdateSeq_ (0)
    , j_ (journal)
{
    assert (majorityFraction_ != 0);

    std::lock_guard <std::mutex> sl (mutex_);

    for (auto const& a : parseSection(supported))
    {
        if (auto s = add (a.first))
        {
            JLOG (j_.debug()) <<
                "Amendment " << a.first << " is supported.";

            if (!a.second.empty ())
                s->name = a.second;

            s->supported = true;
        }
    }

    for (auto const& a : parseSection (enabled))
    {
        if (auto s = add (a.first))
        {
            JLOG (j_.debug()) <<
                "Amendment " << a.first << " is enabled.";

            if (!a.second.empty ())
                s->name = a.second;

            s->supported = true;
            s->enabled = true;
        }
    }

    for (auto const& a : parseSection (vetoed))
    {
        // Unknown amendments are effectively vetoed already
        if (auto s = get (a.first))
        {
            JLOG (j_.info()) <<
                "Amendment " << a.first << " is vetoed.";

            if (!a.second.empty ())
                s->name = a.second;

            s->vetoed = true;
        }
    }
}

CRNState*
CRNRoundImpl::add (uint256 const& amendmentHash)
{
    // call with the mutex held
    return &crnMap_[amendmentHash];
}

CRNState*
CRNRoundImpl::get (uint256 const& amendmentHash)
{
    // call with the mutex held
    auto ret = crnMap_.find (amendmentHash);

    if (ret == crnMap_.end())
        return nullptr;

    return &ret->second;
}

uint256
CRNRoundImpl::find (std::string const& name)
{
    std::lock_guard <std::mutex> sl (mutex_);

    for (auto const& e : crnMap_)
    {
        if (name == e.second.name)
            return e.first;
    }

    return {};
}

bool
CRNRoundImpl::veto (uint256 const& amendment)
{
    std::lock_guard <std::mutex> sl (mutex_);
    auto s = add (amendment);

    if (s->vetoed)
        return false;
    s->vetoed = true;
    return true;
}

bool
CRNRoundImpl::unVeto (uint256 const& amendment)
{
    std::lock_guard <std::mutex> sl (mutex_);
    auto s = get (amendment);

    if (!s || !s->vetoed)
        return false;
    s->vetoed = false;
    return true;
}

bool
CRNRoundImpl::enable (uint256 const& amendment)
{
    std::lock_guard <std::mutex> sl (mutex_);
    auto s = add (amendment);

    if (s->enabled)
        return false;

    s->enabled = true;
    return true;
}

bool
CRNRoundImpl::disable (uint256 const& amendment)
{
    std::lock_guard <std::mutex> sl (mutex_);
    auto s = get (amendment);

    if (!s || !s->enabled)
        return false;

    s->enabled = false;
    return true;
}

bool
CRNRoundImpl::isEnabled (uint256 const& amendment)
{
    std::lock_guard <std::mutex> sl (mutex_);
    auto s = get (amendment);
    return s && s->enabled;
}

bool
CRNRoundImpl::isSupported (uint256 const& amendment)
{
    std::lock_guard <std::mutex> sl (mutex_);
    auto s = get (amendment);
    return s && s->supported;
}

std::vector <uint256>
CRNRoundImpl::doValidation (
    std::set<uint256> const& enabled)
{
    // Get the list of amendments we support and do not
    // veto, but that are not already enabled
    std::vector <uint256> amendments;
    amendments.reserve (crnMap_.size());

    {
        std::lock_guard <std::mutex> sl (mutex_);
        for (auto const& e : crnMap_)
        {
            if (e.second.supported && ! e.second.vetoed &&
                (enabled.count (e.first) == 0))
            {
                amendments.push_back (e.first);
            }
        }
    }

    if (! amendments.empty())
        std::sort (amendments.begin (), amendments.end ());

    return amendments;
}

std::vector <uint256>
CRNRoundImpl::getDesired ()
{
    // Get the list of amendments we support and do not veto
    return doValidation({});
}

std::map <uint256, std::uint32_t>
CRNRoundImpl::doVoting (
    NetClock::time_point closeTime,
    std::set<uint256> const& enabledAmendments,
    majorityAmendments_t const& majorityAmendments,
    ValidationSet const& valSet)
{
    JLOG (j_.trace()) <<
        "voting at " << closeTime.time_since_epoch().count() <<
        ": " << enabledAmendments.size() <<
        ", " << majorityAmendments.size() <<
        ", " << valSet.size();

    auto vote = std::make_unique <CRNSet> ();

    // process validations for ledger before flag ledger
    for (auto const& entry : valSet)
    {
        if (entry.second->isTrusted ())
        {
            std::set<uint256> ballot;

            if (entry.second->isFieldPresent (sfAmendments))
            {
                auto const choices =
                    entry.second->getFieldV256 (sfAmendments);
                ballot.insert (choices.begin (), choices.end ());
            }

            vote->tally (ballot);
        }
    }

    vote->mThreshold = std::max(1,
        (vote->mTrustedValidations * majorityFraction_) / 256);

    JLOG (j_.debug()) <<
        "Received " << vote->mTrustedValidations <<
        " trusted validations, threshold is: " << vote->mThreshold;

    // Map of amendments to the action to be taken for each one. The action is
    // the value of the flags in the pseudo-transaction
    std::map <uint256, std::uint32_t> actions;

    {
        std::lock_guard <std::mutex> sl (mutex_);

        // process all amendments we know of
        for (auto const& entry : crnMap_)
        {
            NetClock::time_point majorityTime = {};

            bool const hasValMajority =
                (vote->votes (entry.first) >= vote->mThreshold);

            {
                auto const it = majorityAmendments.find (entry.first);
                if (it != majorityAmendments.end ())
                    majorityTime = it->second;
            }

            if (enabledAmendments.count (entry.first) != 0)
            {
                JLOG (j_.debug()) <<
                    entry.first << ": amendment already enabled";
            }
        }

        // Stash for reporting
        lastVote_ = std::move(vote);
    }

    return actions;
}

bool
CRNRoundImpl::needValidatedLedger (LedgerIndex ledgerSeq)
{
    std::lock_guard <std::mutex> sl (mutex_);

    // Is there a ledger in which an amendment could have been enabled
    // between these two ledger sequences?

    return ((ledgerSeq - 1) / 256) != ((lastUpdateSeq_ - 1) / 256);
}

void
CRNRoundImpl::doValidatedLedger (
    LedgerIndex ledgerSeq,
    std::set<uint256> const& enabled)
{
    std::lock_guard <std::mutex> sl (mutex_);

    for (auto& e : crnMap_)
        e.second.enabled = (enabled.count (e.first) != 0);
}

void
CRNRoundImpl::setJson (Json::Value& v, const uint256& id, const CRNState& fs)
{
    if (!fs.name.empty())
        v[jss::name] = fs.name;

    v[jss::supported] = fs.supported;
    v[jss::vetoed] = fs.vetoed;
    v[jss::enabled] = fs.enabled;

    if (!fs.enabled && lastVote_)
    {
        auto const votesTotal = lastVote_->mTrustedValidations;
        auto const votesNeeded = lastVote_->mThreshold;
        auto const votesFor = lastVote_->votes (id);

        v[jss::count] = votesFor;
        v[jss::validations] = votesTotal;

        if (votesNeeded)
        {
            v[jss::vote] = votesFor * 256 / votesNeeded;
            v[jss::threshold] = votesNeeded;
        }
    }
}

Json::Value
CRNRoundImpl::getJson (int)
{
    Json::Value ret(Json::objectValue);
    {
        std::lock_guard <std::mutex> sl(mutex_);
        for (auto const& e : crnMap_)
        {
            setJson (ret[to_string (e.first)] = Json::objectValue,
                e.first, e.second);
        }
    }
    return ret;
}

Json::Value
CRNRoundImpl::getJson (uint256 const& amendmentID)
{
    Json::Value ret = Json::objectValue;
    Json::Value& jAmendment = (ret[to_string (amendmentID)] = Json::objectValue);

    {
        std::lock_guard <std::mutex> sl(mutex_);
        auto a = add (amendmentID);
        setJson (jAmendment, amendmentID, *a);
    }

    return ret;
}

std::unique_ptr<CRNRound> make_CRNRound (
    std::chrono::seconds majorityTime,
    int majorityFraction,
    Section const& supported,
    Section const& enabled,
    Section const& vetoed,
    beast::Journal journal)
{
    return std::make_unique<CRNRoundImpl> (
        majorityTime,
        majorityFraction,
        supported,
        enabled,
        vetoed,
        journal);
}

}  // casinocoin
