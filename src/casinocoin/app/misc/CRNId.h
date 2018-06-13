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
    2018-05-30  ajochems        Setting properties from config file
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
#include <casinocoin/app/ledger/LedgerMaster.h>

namespace casinocoin {

class CRNId
{
public:
    CRNId(const CRNId&) = delete;

    CRNId(Section const& relaynodeConfig,
          beast::Journal j,
          LedgerMaster& ledgerMaster)
        : j_(j)
        , m_ledgerMaster (ledgerMaster)
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
           beast::Journal j,
           LedgerMaster& ledgerMaster)
        : pubKey_(pubKey)
        , domain_(domain)
        , signature_(domainSignature)
        , j_(j)
        , m_ledgerMaster (ledgerMaster)
    {
    }

    Json::Value json() const
    {
        Json::Value ret = Json::objectValue;
        ret[jss::crn_public_key] = toBase58(TOKEN_NODE_PUBLIC, pubKey_);
        ret[jss::crn_domain_name] = domain_;
        ret[jss::crn_activated] = activated();
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

    bool activated() const
    {
        bool activated_ = false;
        // get the account id for the CRN
        boost::optional <AccountID> accountID = calcAccountID(pubKey_);
        // get the last validated ledger
        auto const ledger = m_ledgerMaster.getValidatedLedger();
        if(ledger)
        {
            auto const sleAccepted = ledger->read(keylet::account(*accountID));
            if (sleAccepted)
            {
                STAmount amount = sleAccepted->getFieldAmount (sfBalance);
                JLOG(j_.info()) << "CRN Account Balance: " << amount.getFullText ();
                // check if the account balance is >= defined reserve
                if(amount >= 100000000000000)
                {
                    activated_ = true;
                }
            }
        }
        else
        {
            JLOG(j_.info()) << "CRN No Validated Ledger for Activated.";
        }
        return activated_;
    }

    bool onOverlayMessage(const std::shared_ptr<protocol::TMReportState> &m) const
    {
        JLOG(j_.info()) << "CRNId::onMessage TMReportState.";

        PublicKey incomingPubKey;
        if (m->has_crnpubkey())
            incomingPubKey= PublicKey(Slice(m->crnpubkey().data(), m->crnpubkey().size()));
        else
            JLOG(j_.info()) << "CRNId::onOverlayMessage TMReportState crnPubKey missing in msg";

        if (!(pubKey_ == incomingPubKey))
        {
            JLOG(j_.warn()) << "CRNId::onMessage TMReportState public key mismatch"
                                    << " incomingPK: " << incomingPubKey
                                    << " ourPK: " << pubKey_;
            return false;
        }

        if (!m->has_domain())
        {
            JLOG(j_.info()) << "CRNId::onOverlayMessage TMReportState domain missing in msg";
            return false;
        }
        if (domain_ != m->domain())
        {
            JLOG(j_.warn()) << "CRNId::onMessage TMReportState domain mismatch"
                                    << " incoming domain: " << m->domain()
                                    << " our domain: " << domain_;
            return false;
        }

        if (!m->has_signature())
        {
            JLOG(j_.info()) << "CRNId::onOverlayMessage TMReportState signature missing in msg";
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
    LedgerMaster& m_ledgerMaster;
};

}
#endif // CASINOCOIN_PROTOCOL_CRNID_H
