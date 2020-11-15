// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_FEERATE_H
#define BITCOIN_POLICY_FEERATE_H

#include <amount.h>
#include <serialize.h>

#include <string>

const std::string CURRENCY_UNIT = "BTC"; // One formatted unit
const std::string CURRENCY_ATOM = "sat"; // One indivisible minimum value unit

/* Used to determine type of fee estimation requested */
enum class FeeEstimateMode {
    UNSET,        //!< Use default settings based on other criteria
    ECONOMICAL,   //!< Force estimateSmartFee to use non-conservative estimates
    CONSERVATIVE, //!< Force estimateSmartFee to use conservative estimates
    BTC_KVB,      //!< Use BTC/kvB fee rate unit
    SAT_VB,       //!< Use sat/vB fee rate unit
};

/**
 * Fee rate in satoshis per kilobyte: CAmount / kB
 */
class CFeeRate
{
private:
    CAmount nSatoshisPerK; // unit is satoshis-per-1,000-bytes

public:
    /** Fee rate of 0 satoshis per kB */
    CFeeRate() : nSatoshisPerK(0) { }
    template<typename I>
    explicit CFeeRate(const I _nSatoshisPerK): nSatoshisPerK(_nSatoshisPerK) {
        // We've previously had bugs creep in from silent double->int conversion...
        static_assert(std::is_integral<I>::value, "CFeeRate should be used without floats");
    }
    static CFeeRate FromSatB(CAmount fee_rate);
    static CFeeRate FromBtcKb(CAmount fee_rate);
    /** Constructor for a fee rate in satoshis per kvB (sat/kvB). The size in bytes must not exceed (2^63 - 1).
     *
     *  Passing an nBytes value of COIN (1e8) returns a fee rate in satoshis per vB (sat/vB),
     *  e.g. (nFeePaid * 1e8 / 1e3) == (nFeePaid / 1e5),
     *  where 1e5 is the ratio to convert from BTC/kvB to sat/vB.
     *
     *  @param[in] nFeePaid  CAmount fee rate to construct with
     *  @param[in] nBytes    size_t bytes (units) to construct with
     *  @returns   fee rate
     */
    CFeeRate(const CAmount& nFeePaid, size_t nBytes);
    /**
     * Return the fee in satoshis for the given size in bytes.
     */
    CAmount GetFee(size_t nBytes) const;
    /**
     * Return the fee in satoshis for a size of 1000 bytes
     */
    CAmount GetFeePerK() const { return GetFee(1000); }
    friend bool operator<(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK < b.nSatoshisPerK; }
    friend bool operator>(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK > b.nSatoshisPerK; }
    friend bool operator==(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK == b.nSatoshisPerK; }
    friend bool operator<=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK <= b.nSatoshisPerK; }
    friend bool operator>=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK >= b.nSatoshisPerK; }
    friend bool operator!=(const CFeeRate& a, const CFeeRate& b) { return a.nSatoshisPerK != b.nSatoshisPerK; }
    CFeeRate& operator+=(const CFeeRate& a) { nSatoshisPerK += a.nSatoshisPerK; return *this; }
    /**
     * Return the fee rate in BTC/kvB or sat/vB, with or without units, as a string.
     *
     *  @param[in] mode        FeeEstimateMode::SAT_VB or FeeEstimateMode::BTC_KVB
     *  @param[in] with_units  bool whether to append the fee rate units ("sat/vB" or "BTC/kvB")
     */
    std::string ToString(FeeEstimateMode mode = FeeEstimateMode::BTC_KVB, bool with_units = true) const;

    SERIALIZE_METHODS(CFeeRate, obj) { READWRITE(obj.nSatoshisPerK); }
};

/** Construct a CFeeRate from a CAmount in sat/vB */
inline CFeeRate CFeeRate::FromSatB(CAmount fee_rate) { return CFeeRate(fee_rate, COIN); }

/** Construct a CFeeRate from a CAmount in BTC/kvB */
inline CFeeRate CFeeRate::FromBtcKb(CAmount fee_rate) { return CFeeRate(fee_rate, 1000); }

#endif //  BITCOIN_POLICY_FEERATE_H
