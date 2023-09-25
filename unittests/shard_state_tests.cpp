#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <test_contracts.hpp>
#define TEST tester
using namespace eosio;
using namespace testing;
using namespace chain;

BOOST_AUTO_TEST_SUITE(shard_state_tests)

BOOST_FIXTURE_TEST_CASE(query_account_object_from_share_db_test,TEST){
    //system account create account test on main.
    create_account("test"_n);
    produce_block();
    //account test create account hello on shard1.
    //query account test from shard db.
    create_account_on_subshard("hello"_n, "test"_n);
    produce_block();
    
    produce_block();
}

BOOST_AUTO_TEST_SUITE_END()