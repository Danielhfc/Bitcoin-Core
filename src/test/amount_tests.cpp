// Copyright (c) 2016-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <policy/feerate.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(amount_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(MoneyRangeTest)
{
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(-1)), false);
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(0)), true);
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(1)), true);
    BOOST_CHECK_EQUAL(MoneyRange(MAX_MONEY), true);
    BOOST_CHECK_EQUAL(MoneyRange(MAX_MONEY + CAmount(1)), false);
}

BOOST_AUTO_TEST_CASE(GetFeeTest)
{
    CFeeRate feeRate, altFeeRate;

    feeRate = CFeeRate(0);
    // Must always return 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e5), CAmount(0));

    feeRate = CFeeRate(1000);
    // Must always just return the arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(121));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(999));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(1e3));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(9e3));

    feeRate = CFeeRate(-1000);
    // Must always just return -1 * arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(-1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(-121));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(-999));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(-1e3));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(-9e3));

    feeRate = CFeeRate(123);
    // Truncates the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(8), CAmount(1)); // Special case: returns 1 instead of 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(9), CAmount(1));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(14));
    BOOST_CHECK_EQUAL(feeRate.GetFee(122), CAmount(15));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(122));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(1107));

    feeRate = CFeeRate(-123);
    // Truncates the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(8), CAmount(-1)); // Special case: returns -1 instead of 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(9), CAmount(-1));

    // check alternate constructor
    feeRate = CFeeRate(1000);
    altFeeRate = CFeeRate(feeRate);
    BOOST_CHECK_EQUAL(feeRate.GetFee(100), altFeeRate.GetFee(100));
}

BOOST_AUTO_TEST_CASE(CFeeRateConstructorTest)
{
    // Test CFeeRate(CAmount fee_rate, size_t bytes) constructor
    // full constructor
    BOOST_CHECK(CFeeRate(CAmount(-1), 0) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(0), 0) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(1), 0) == CFeeRate(0));
    // default value
    BOOST_CHECK(CFeeRate(CAmount(-1), 1000) == CFeeRate(-1));
    BOOST_CHECK(CFeeRate(CAmount(0), 1000) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(1), 1000) == CFeeRate(1));
    // lost precision (can only resolve satoshis per kB)
    BOOST_CHECK(CFeeRate(CAmount(1), 1001) == CFeeRate(0));
    BOOST_CHECK(CFeeRate(CAmount(2), 1001) == CFeeRate(1));
    // some more integer checks
    BOOST_CHECK(CFeeRate(CAmount(26), 789) == CFeeRate(32));
    BOOST_CHECK(CFeeRate(CAmount(27), 789) == CFeeRate(34));
    // Maximum size in bytes, should not crash
    CFeeRate(MAX_MONEY, std::numeric_limits<size_t>::max() >> 1).GetFeePerK();
}

BOOST_AUTO_TEST_CASE(CFeeRateNamedConstructorsTest)
{
    // Test CFeerate(CAmount fee_rate, FeeEstimatemode mode) constructor
    // ...with BTC/kvB, returns same values as CFeeRate(CAmount fee_rate)
    BOOST_CHECK(CFeeRate::FromBtcKb(CAmount(-1)) == CFeeRate(-1));
    BOOST_CHECK(CFeeRate::FromBtcKb(CAmount(0)) == CFeeRate(0));
    BOOST_CHECK(CFeeRate::FromBtcKb(CAmount(1)) == CFeeRate(1));
    BOOST_CHECK(CFeeRate::FromBtcKb(CAmount(26)) == CFeeRate(26));
    BOOST_CHECK(CFeeRate::FromBtcKb(CAmount(123)) == CFeeRate(123));
    // ...with sat/vB, returns values that are 1e5 smaller
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(-100000)) == CFeeRate(-1));
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(-99999)) == CFeeRate(0));
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(0)) == CFeeRate(0));
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(99999)) == CFeeRate(0));
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(100000)) == CFeeRate(1));
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(100001)) == CFeeRate(1));
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(2690000)) == CFeeRate(26));
    BOOST_CHECK(CFeeRate::FromSatB(CAmount(123456789)) == CFeeRate(1234));
}

BOOST_AUTO_TEST_CASE(BinaryOperatorTest)
{
    CFeeRate a, b;
    a = CFeeRate(1);
    b = CFeeRate(2);
    BOOST_CHECK(a < b);
    BOOST_CHECK(b > a);
    BOOST_CHECK(a == a);
    BOOST_CHECK(a <= b);
    BOOST_CHECK(a <= a);
    BOOST_CHECK(b >= a);
    BOOST_CHECK(b >= b);
    // a should be 0.00000002 BTC/kvB now
    a += a;
    BOOST_CHECK(a == b);
}

BOOST_AUTO_TEST_CASE(ToStringTest)
{
    CFeeRate fee_rate{CFeeRate(1)};
    BOOST_CHECK_EQUAL(fee_rate.ToString(), "0.00000001 BTC/kvB");
    BOOST_CHECK_EQUAL(fee_rate.ToString(FeeEstimateMode::BTC_KVB), "0.00000001 BTC/kvB");
    BOOST_CHECK_EQUAL(fee_rate.ToString(FeeEstimateMode::SAT_VB), "0.001 sat/vB");

    BOOST_CHECK_EQUAL(fee_rate.ToString(FeeEstimateMode::BTC_KVB, /* with_units */ true), "0.00000001 BTC/kvB");
    BOOST_CHECK_EQUAL(fee_rate.ToString(FeeEstimateMode::SAT_VB, /* with_units */ true), "0.001 sat/vB");

    BOOST_CHECK_EQUAL(CFeeRate(1).ToString(FeeEstimateMode::SAT_VB, /* with_units */ false), "0.001");
    BOOST_CHECK_EQUAL(CFeeRate(70).ToString(FeeEstimateMode::SAT_VB, /* with_units */ false), "0.070");
    BOOST_CHECK_EQUAL(CFeeRate(3141).ToString(FeeEstimateMode::SAT_VB, /* with_units */ false), "3.141");
    BOOST_CHECK_EQUAL(CFeeRate(10002).ToString(FeeEstimateMode::SAT_VB, /* with_units */ false), "10.002");
    BOOST_CHECK_EQUAL(CFeeRate(3141).ToString(FeeEstimateMode::BTC_KVB, /* with_units */ false), "0.00003141");
    BOOST_CHECK_EQUAL(CFeeRate(10002).ToString(FeeEstimateMode::BTC_KVB, /* with_units */ false), "0.00010002");
}

BOOST_AUTO_TEST_SUITE_END()
