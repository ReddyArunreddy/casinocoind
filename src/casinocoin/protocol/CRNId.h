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
    2018-05-15  jrojek          Created
*/
//==============================================================================

#ifndef CASINOCOIN_PROTOCOL_CRNID_H
#define CASINOCOIN_PROTOCOL_CRNID_H

#include <casinocoin/core/ConfigSections.h>
#include <casinocoin/core/Config.h>

namespace casinocoin {

// jrojek TODO feel that class with section contents, add section to CFG file (possibly ajohems?)
class CRNId
{
public:
    CRNId(Section const& crnConfig)
    {

    }

    std::string const& getNodePubKey() const
    {
        return nodePubKey_;
    }

    std::string const& getNodeIP() const
    {
        return nodeIP_;
    }

    std::string const& getNodeDomain() const
    {
        return nodeDomain_;
    }

private:
    std::string nodePubKey_;
    std::string nodeIP_;
    std::string nodeDomain_;
};
}
#endif // CASINOCOIN_PROTOCOL_CRNID_H
