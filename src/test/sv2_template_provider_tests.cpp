#include <sv2_template_provider.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sv2_template_provider_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(SetupConnection_test)
{
    uint8_t expected[]{
        0x03, // protocol
        0x02, 0x00, // min_version
        0x02, 0x00, // max_version
        0x01, 0x00, 0x00, 0x00, // flags
        0x07, 0x30, 0x2e, 0x30, 0x2e, 0x30, 0x2e, 0x30, // endpoint_host
        0x61, 0x21, // endpoint_port
        0x07, 0x42, 0x69, 0x74, 0x6d, 0x61, 0x69, 0x6e, // vendor
        0x08, 0x53, 0x39, 0x69, 0x20, 0x31, 0x33, 0x2e, 0x35, // hardware_version 
        0x1c, 0x62, 0x72, 0x61, 0x69, 0x69, 0x6e, 0x73, 0x2d, 0x6f, 0x73, 0x2d, 0x32, 0x30,
        0x31, 0x38, 0x2d, 0x30, 0x39, 0x2d, 0x32, 0x32, 0x2d, 0x31, 0x2d, 0x68, 0x61, 0x73,
        0x68, //firmware
        0x10, 0x73, 0x6f, 0x6d, 0x65, 0x2d, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65, 0x2d, 0x75,
        0x75, 0x69, 0x64, // device_id
    };
    BOOST_CHECK_EQUAL(sizeof(expected), 82);

    CDataStream ss(expected, SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_EQUAL(ss.size(), 82);

    SetupConnection setup_conn;
    ss >> setup_conn;

    BOOST_CHECK_EQUAL(setup_conn.m_protocol, 3);
    BOOST_CHECK_EQUAL(setup_conn.m_min_version, 2);
    BOOST_CHECK_EQUAL(setup_conn.m_max_version, 2);
    BOOST_CHECK_EQUAL(setup_conn.m_flags, 1);
    BOOST_CHECK_EQUAL(setup_conn.m_endpoint_host, "0.0.0.0");
    BOOST_CHECK_EQUAL(setup_conn.m_endpoint_port, 8545);
    BOOST_CHECK_EQUAL(setup_conn.m_vendor, "Bitmain");
    BOOST_CHECK_EQUAL(setup_conn.m_hardware_version, "S9i 13.5");
    BOOST_CHECK_EQUAL(setup_conn.m_firmware, "braiins-os-2018-09-22-1-hash");
    BOOST_CHECK_EQUAL(setup_conn.m_device_id, "some-device-uuid");
}

BOOST_AUTO_TEST_CASE(SetupConnectionSuccess_test)
{
    uint8_t expected[]{
       0x02, 0x00, // used_version
       0x03, 0x00, 0x00, 0x00, // flags
    };

    SetupConnectionSuccess setup_conn_success{2, 3};

    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << setup_conn_success;
    BOOST_CHECK_EQUAL(ss.size(), 6);

    std::vector<uint8_t> bytes;
    for (int i = 0; i < sizeof(expected); ++i) {
        uint8_t b;
        ss >> b;
        bytes.push_back(b);
    }
    BOOST_CHECK_EQUAL(bytes.size(), 6);
    BOOST_CHECK(std::equal(bytes.begin(), bytes.end(), expected));
}

BOOST_AUTO_TEST_SUITE_END()
