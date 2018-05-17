//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2015 Ripple Labs Inc.

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
    2018-05-17  ajochems        Initial version
*/
//==============================================================================

#include <casinocoin/app/misc/CRNList.h>
#include <casinocoin/basics/Slice.h>
#include <casinocoin/basics/StringUtilities.h>
#include <casinocoin/json/json_reader.h>
#include <beast/core/detail/base64.hpp>
#include <boost/regex.hpp>

namespace casinocoin {

CRNList::CRNList (
    TimeKeeper& timeKeeper,
    beast::Journal j)
    : timeKeeper_ (timeKeeper)
    , j_ (j)
{
}

CRNList::~CRNList()
{
}

bool
CRNList::load (
    PublicKey const& localSigningKey,
    std::vector<std::string> const& configKeys,
    std::vector<std::string> const& publisherKeys)
{
    static boost::regex const re (
        "[[:space:]]*"            // skip leading whitespace
        "([[:alnum:]]+)"          // node identity
        "(?:"                     // begin optional comment block
        "[[:space:]]+"            // (skip all leading whitespace)
        "(?:"                     // begin optional comment
        "(.*[^[:space:]]+)"       // the comment
        "[[:space:]]*"            // (skip all trailing whitespace)
        ")?"                      // end optional comment
        ")?"                      // end optional comment block
    );

    boost::unique_lock<boost::shared_mutex> read_lock{mutex_};

    JLOG (j_.debug()) << "Loading configured trusted CRN list publisher keys";

    std::size_t count = 0;
    for (auto key : publisherKeys)
    {
        JLOG (j_.trace()) <<
            "Processing '" << key << "'";

        auto const ret = strUnHex (key);

        if (! ret.second || ! ret.first.size ())
        {
            JLOG (j_.error()) << "Invalid CRN list publisher key: " << key;
            return false;
        }

        auto id = PublicKey(Slice{ ret.first.data (), ret.first.size() });

        if (publisherLists_.count(id))
        {
            JLOG (j_.warn()) <<
                "Duplicate CRN list publisher key: " << key;
            continue;
        }

        publisherLists_[id].available = false;
        ++count;
    }

    JLOG (j_.debug()) << "Loaded " << count << " keys";

    // TODO !!! localPubKey_ = validatorManifests_.getMasterKey (localSigningKey);

    // Treat local CRN key as though it was listed in the config
    if (localPubKey_.size())
        keyListings_.insert ({ localPubKey_, 1 });

    JLOG (j_.debug()) << "Loading configured CRN keys";

    count = 0;
    PublicKey local;
    for (auto const& n : configKeys)
    {
        JLOG (j_.trace()) <<
            "Processing '" << n << "'";

        boost::smatch match;

        if (!boost::regex_match (n, match, re))
        {
            JLOG (j_.error()) <<
                "Malformed entry: '" << n << "'";
            return false;
        }

        auto const id = parseBase58<PublicKey>(
            TokenType::TOKEN_NODE_PUBLIC, match[1]);

        if (!id)
        {
            JLOG (j_.error()) << "Invalid node identity: " << match[1];
            return false;
        }

        // Skip local key which was already added
        if (*id == localPubKey_ || *id == localSigningKey)
            continue;

        auto ret = keyListings_.insert ({*id, 1});
        if (! ret.second)
        {
            JLOG (j_.warn()) << "Duplicate node identity: " << match[1];
            continue;
        }
        publisherLists_[local].list.emplace_back (std::move(*id));
        publisherLists_[local].available = true;
        ++count;
    }

    JLOG (j_.debug()) <<
        "Loaded " << count << " entries";

    return true;
}

bool
CRNList::listed (
    PublicKey const& identity) const
{
    boost::shared_lock<boost::shared_mutex> read_lock{mutex_};

    // auto const pubKey;
    // return keyListings_.find (pubKey) != keyListings_.end ();
    return true;
}

boost::optional<PublicKey>
CRNList::getListedKey (PublicKey const& identity) const
{
    boost::shared_lock<boost::shared_mutex> read_lock{mutex_};

    // auto const pubKey;
    // if (keyListings_.find (pubKey) != keyListings_.end ())
    //     return pubKey;
    return boost::none;
}

PublicKey
CRNList::localPublicKey () const
{
    boost::shared_lock<boost::shared_mutex> read_lock{mutex_};
    return localPubKey_;
}


void
CRNList::for_each_listed (
    std::function<void(PublicKey const&, bool)> func) const
{
    boost::shared_lock<boost::shared_mutex> read_lock{mutex_};

    // for (auto const& v : keyListings_)
    //     func (v.first, trusted(v.first));
}


} // casinocoin
