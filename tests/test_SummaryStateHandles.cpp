#include <opm/input/eclipse/Schedule/SummaryState.hpp>

#define BOOST_TEST_MODULE SummaryStateHandleTest
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(HandleRegistration) {
    Opm::SummaryState st{std::time_t{0}};
    
    auto h1 = st.register_well_var("PROD-1", "WOPR");
    auto h2 = st.register_well_var("PROD-1", "WWCT");
    auto h3 = st.register_well_var("PROD-2", "WOPR");
    
    // Handles should be valid and unique
    BOOST_CHECK(h1.is_valid());
    BOOST_CHECK(h2.is_valid());
    BOOST_CHECK(h3.is_valid());
    BOOST_CHECK(h1.index != h2.index);
    BOOST_CHECK(h2.index != h3.index);
    
    // Re-registering should return same handle
    auto h1_again = st.register_well_var("PROD-1", "WOPR");
    BOOST_CHECK(h1 == h1_again);
}

BOOST_AUTO_TEST_CASE(HandleAccessAndUpdate) {
    Opm::SummaryState st{std::time_t{0}};
    
    auto h = st.register_well_var("PROD-1", "WOPR");
    
    // Initial value should be zero
    BOOST_CHECK_EQUAL(st.get(h), 0.0);
    
    // Update via handle
    st.update(h, 1500.0);
    BOOST_CHECK_EQUAL(st.get(h), 1500.0);
    
    // Direct assignment via operator[]
    st[h] = 2000.0;
    BOOST_CHECK_EQUAL(st.get(h), 2000.0);
}

BOOST_AUTO_TEST_CASE(HandleStringAPIParity) {
    Opm::SummaryState st{std::time_t{0}};
    
    // Update via handle
    auto h = st.register_well_var("PROD-1", "WOPR");
    st.update(h, 1500.0);
    
    // Read via string API should give same value
    BOOST_CHECK_EQUAL(st.get_well_var("PROD-1", "WOPR"), 1500.0);
    
    // Update via string API
    st.update_well_var("PROD-1", "WOPR", 200.0);
    
    // Read via handle should see the update
    BOOST_CHECK_EQUAL(st.get(h), 1700.0); // Should be 1500 + 200
}

BOOST_AUTO_TEST_CASE(TotalAccumulation) {
    Opm::SummaryState st{std::time_t{0}};
    
    // OPT is a total (should accumulate)
    auto h_total = st.register_well_var("PROD-1", "WOPT");
    
    st.update(h_total, 100.0);
    BOOST_CHECK_EQUAL(st.get(h_total), 100.0);
    
    st.update(h_total, 50.0);
    BOOST_CHECK_EQUAL(st.get(h_total), 150.0); // Accumulated
    
    // OPR is a rate (should not accumulate)
    auto h_rate = st.register_well_var("PROD-1", "WOPR");
    
    st.update(h_rate, 1000.0);
    BOOST_CHECK_EQUAL(st.get(h_rate), 1000.0);
    
    st.update(h_rate, 1500.0);
    BOOST_CHECK_EQUAL(st.get(h_rate), 1500.0); // Replaced, not accumulated
}

BOOST_AUTO_TEST_CASE(ConnectionHandles) {
    Opm::SummaryState st{std::time_t{0}};
    
    // Register connection variables
    auto h1 = st.register_conn_var("PROD-1", "COPR", 100);
    auto h2 = st.register_conn_var("PROD-1", "COPR", 101);
    
    st.update(h1, 500.0);
    st.update(h2, 750.0);
    
    BOOST_CHECK_EQUAL(st.get(h1), 500.0);
    BOOST_CHECK_EQUAL(st.get(h2), 750.0);
    
    // Verify via string API
    BOOST_CHECK_EQUAL(st.get_conn_var("PROD-1", "COPR", 100), 500.0);
    BOOST_CHECK_EQUAL(st.get_conn_var("PROD-1", "COPR", 101), 750.0);
}