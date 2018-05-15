//------------------------------------------------------------------------------
/*
    This file is part of casinocoind: https://github.com/casinocoin/casinocoind

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
    2018-05-15  jrojek        created
*/
//==============================================================================


#ifndef CASINOCOIN_TX_CRNREPORT_H
#define CASINOCOIN_TX_CRNREPORT_H

#include <casinocoin/app/tx/impl/Transactor.h>
#include <casinocoin/basics/Log.h>
#include <casinocoin/core/Config.h>
#include <casinocoin/protocol/Indexes.h>
#include <casinocoin/protocol/Quality.h>
#include <casinocoin/protocol/TxFlags.h>

namespace casinocoin {

class CRNReport
    : public Transactor
{
public:
    CRNReport (ApplyContext& ctx)
        : Transactor(ctx)
    {
    }

    static
    std::uint64_t
    calculateBaseFee (
        PreclaimContext const& ctx)
    {
        return 0;
    }

    static
    TER
    preflight (PreflightContext const& ctx);

    TER doApply () override;
};

} // casinocoin

#endif // CASINOCOIN_TX_CRNREPORT_H
