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

#include <casinocoin/basics/Log.h>
#include <casinocoin/beast/utility/Journal.h>
#include <casinocoin/core/ConfigSections.h>
#include <casinocoin/core/Config.h>
#include <casinocoin/overlay/impl/ProtocolMessage.h>
#include <casinocoin/protocol/PublicKey.h>
#include <casinocoin/protocol/AccountID.h>
#include <casinocoin/protocol/JsonFields.h>


namespace casinocoin {

// jrojek TODO fill that class with section contents, add section to CFG file (possibly ajohems?)
class CRNId
{
public:
    CRNId(const CRNId&) = delete;

    CRNId(Section const& relaynodeConfig,
          beast::Journal j)
        : j_(j)
    {
        std::pair <std::string, bool> domainName = relaynodeConfig.find("domain");
        std::pair <std::string, bool> publicKey = relaynodeConfig.find("publickey");
        std::pair <std::string, bool> signature = relaynodeConfig.find("signature");
        if(domainName.second && publicKey.second && signature.second)
        {
            boost::optional<PublicKey> crnPublicKey = parseBase58<PublicKey>(TokenType::TOKEN_NODE_PUBLIC, publicKey.first);
            pubKey_ = *crnPublicKey;
            domain_ = domainName.first;
            signature_ = signature.first;
        }
        else
        {
            JLOG(j_.warn()) << "failed to parse relaynode config section";
        }
    }

    CRNId(PublicKey const& pubKey,
           std::string const& domain,
           std::string const& domainSignature,
           beast::Journal j)
        : pubKey_(pubKey)
        , domain_(domain)
        , signature_(domainSignature)
        , j_(j)
    {
    }

    Json::Value json() const
    {
        Json::Value ret = Json::objectValue;
        ret[jss::crn_public_key] = toBase58(TOKEN_NODE_PUBLIC, pubKey_);
        ret[jss::crn_domain_name] = domain_;
        ret[jss::crn_domain_signature] = signature_;
        return ret;
    }
    PublicKey const& publicKey() const
    {
        return pubKey_;
    }

    std::string const& domain() const
    {
        return domain_;
    }

    std::string const& signature() const
    {
        return signature_;
    }

    bool onOverlayMessage(const std::shared_ptr<protocol::TMReportState> &m) const
    {
        JLOG(j_.debug()) << "CRNId::onMessage TMReportState.";

        PublicKey incomingPubKey = PublicKey(Slice(m->crnpubkey().data(), m->crnpubkey().size()));
        if (!(pubKey_ == incomingPubKey))
        {
            JLOG(j_.warn()) << "CRNId::onMessage TMReportState public key mismatch"
                                    << " incomingPK: " << incomingPubKey
                                    << " ourPK: " << pubKey_;
            return false;
        }

        if (domain_ != m->domain())
        {
            JLOG(j_.warn()) << "CRNId::onMessage TMReportState domain mismatch"
                                    << " incoming domain: " << m->domain()
                                    << " our domain: " << domain_;
            return false;
        }
        if (signature_ != m->signature())
        {
            JLOG(j_.warn()) << "CRNId::onMessage TMReportState domain mismatch"
                                    << " incoming signature: " << m->signature()
                                    << " our signature: " << signature_;
            return false;
        }

        return true;
    }
private:
    PublicKey pubKey_;
    AccountID accountId_;
    std::string domain_;
    std::string signature_;

    beast::Journal j_;
};

}
#endif // CASINOCOIN_PROTOCOL_CRNID_H
