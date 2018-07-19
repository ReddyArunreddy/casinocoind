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
    2018-07-19  jrojek          Created
*/
//==============================================================================

#include <BeastConfig.h>
#include <casinocoin/protocol/STPerformanceReport.h>
#include <casinocoin/protocol/HashPrefix.h>
#include <casinocoin/basics/contract.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/json/to_string.h>

namespace casinocoin {

STPerformanceReport::STPerformanceReport (SerialIter& sit, bool checkSignature)
    : STObject (getFormat (), sit, sfValidation)
{
    mNodeID = calcNodeID(
        PublicKey(makeSlice (getFieldVL (sfSigningPubKey))));
    assert (mNodeID.isNonZero ());

    if  (checkSignature && !isValid ())
    {
        JLOG (debugLog().error())
            << "Invalid performance report" << getJson (0);
        Throw<std::runtime_error> ("Invalid performance report");
    }
}

STPerformanceReport::STPerformanceReport (
        NetClock::time_point signTime,
        PublicKey const& publicKey)
    : STObject (getFormat (), sfPerformanceReport)
    , mSeen (signTime)
{
    // Does not sign
    setFieldU32 (sfSigningTime, signTime.time_since_epoch().count());

    setFieldVL (sfSigningPubKey, publicKey.slice());
    mNodeID = calcNodeID(publicKey);
    assert (mNodeID.isNonZero ());
}

uint256 STPerformanceReport::sign (SecretKey const& secretKey)
{
    setFlag (vfFullyCanonicalSig);

    auto const signingHash = getSigningHash();
    setFieldVL (sfSignature,
        signDigest (getSignerPublic(), secretKey, signingHash));
    return signingHash;
}

uint256 STPerformanceReport::getSigningHash () const
{
    return STObject::getSigningHash (HashPrefix::performanceReport);
}

NetClock::time_point
STPerformanceReport::getSignTime () const
{
    return NetClock::time_point{NetClock::duration{getFieldU32(sfSigningTime)}};
}

NetClock::time_point STPerformanceReport::getSeenTime () const
{
    return mSeen;
}

std::uint32_t STPerformanceReport::getFlags () const
{
    return getFieldU32 (sfFlags);
}

bool STPerformanceReport::isValid () const
{
    return isValid (getSigningHash ());
}

bool STPerformanceReport::isValid (uint256 const& signingHash) const
{
    try
    {
        return verifyDigest (getSignerPublic(),
            signingHash,
            makeSlice(getFieldVL (sfSignature)),
            getFlags () & vfFullyCanonicalSig);
    }
    catch (std::exception const&)
    {
        JLOG (debugLog().error())
            << "Exception validating performance report";
        return false;
    }
}

PublicKey STPerformanceReport::getSignerPublic () const
{
    return PublicKey(makeSlice (getFieldVL (sfSigningPubKey)));
}

Blob STPerformanceReport::getSignature () const
{
    return getFieldVL (sfSignature);
}

Blob STPerformanceReport::getSerialized () const
{
    Serializer s;
    add (s);
    return s.peekData ();
}

SOTemplate const& STPerformanceReport::getFormat ()
{
    struct FormatHolder
    {
        SOTemplate format;

        FormatHolder ()
        {
            format.push_back (SOElement (sfFlags,               SOE_REQUIRED));
            format.push_back (SOElement (sfSigningTime,         SOE_REQUIRED));
            format.push_back (SOElement (sfSigningPubKey,       SOE_REQUIRED));
            format.push_back (SOElement (sfLedgerSequence,      SOE_OPTIONAL));
            format.push_back (SOElement (sfSignature,           SOE_OPTIONAL));
            format.push_back (SOElement (sfCRNs,                SOE_OPTIONAL));
        }
    };

    static FormatHolder holder;

    return holder.format;
}

} // casinocoin
