//------------------------------------------------------------------------------
/*
    This file is part of casinocoind: https://github.com/casinocoin/casinocoind
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
    2018-05-17  ajochems        Initial version
*/
//==============================================================================

#ifndef CASINOCOIN_APP_MISC_CRNLIST_H_INCLUDED
#define CASINOCOIN_APP_MISC_CRNLIST_H_INCLUDED

#include <casinocoin/basics/Log.h>
#include <casinocoin/basics/UnorderedContainers.h>
#include <casinocoin/core/TimeKeeper.h>
#include <casinocoin/crypto/csprng.h>
#include <casinocoin/protocol/PublicKey.h>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <mutex>
#include <numeric>

namespace casinocoin {

/**
    Trusted CRN List
    -----------------------
    [Description]

*/
class CRNList
{
    struct PublisherList
    {
        bool available;
        std::vector<PublicKey> list;
        std::size_t sequence;
        std::size_t expiration;
    };

    TimeKeeper& timeKeeper_;
    beast::Journal j_;
    boost::shared_mutex mutable mutex_;

    std::atomic<std::size_t> quorum_;
    boost::optional<std::size_t> minimumQuorum_;

    // Published lists stored by publisher master public key
    hash_map<PublicKey, PublisherList> publisherLists_;

    // Listed master public keys with the number of lists they appear on
    hash_map<PublicKey, std::size_t> keyListings_;

    // The current list of trusted master keys
    hash_set<PublicKey> trustedKeys_;

    PublicKey localPubKey_;

public:
    CRNList (
        TimeKeeper& timeKeeper,
        beast::Journal j);
    ~CRNList ();

    /** Load configured trusted keys.

        @param localSigningKey This node's crn public key

        @param configKeys List of trusted keys from config. Each entry consists
        of a base58 encoded crn public key, followed by the nodes domain name.

        @param publisherKeys List of trusted publisher public keys. Each entry
        contains a base58 encoded account public key.

        @par Thread Safety

        May be called concurrently

        @return `false` if an entry is invalid or unparsable
    */
    bool
    load (
        PublicKey const& localSigningKey,
        std::vector<std::string> const& configKeys,
        std::vector<std::string> const& publisherKeys);

    /** Update trusted keys

        Reset the trusted keys based on latest list.

        @par Thread Safety

        May be called concurrently
    */
    template<class KeySet>
    void
    onConsensusStart ();

    /** Returns `true` if public key is included on any lists

        @param identity CRN public key

        @par Thread Safety

        May be called concurrently
    */
    bool
    listed (PublicKey const& identity) const;

    /** Returns listed master public if public key is included on any lists

        @param identity CRN public key

        @return `boost::none` if key is not listed

        @par Thread Safety

        May be called concurrently
    */
    boost::optional<PublicKey>
    getListedKey (PublicKey const& identity) const;


    /** Returns local CRN public key

        @par Thread Safety

        May be called concurrently
    */
    PublicKey
    localPublicKey () const;

    /** Invokes the callback once for every listed CRN public key.

        @note Undefined behavior results when calling CRNList members from within the callback

        The arguments passed into the lambda are:

        @li The CRN public key

        @li A boolean indicating whether this is a trusted key

        @par Thread Safety

        May be called concurrently
    */
    void
    for_each_listed (std::function<void(PublicKey const&, bool)> func) const;

};

//------------------------------------------------------------------------------

template<class KeySet>
void
CRNList::onConsensusStart ()
{
    boost::unique_lock<boost::shared_mutex> lock{mutex_};

    std::multimap<std::size_t, PublicKey> rankedKeys;
    bool localKeyListed = false;

    // "Iterate" the listed keys in random order so that the rank of multiple
    // keys with the same number of listings is not deterministic
    std::vector<std::size_t> indexes (keyListings_.size());
    std::iota (indexes.begin(), indexes.end(), 0);
    std::shuffle (indexes.begin(), indexes.end(), crypto_prng());

    JLOG (j_.debug()) << "CRNList onConsensusStart";

}

} // casinocoin

#endif
