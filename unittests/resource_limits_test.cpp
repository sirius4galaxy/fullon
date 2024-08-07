#include <algorithm>

#include <eosio/chain/config.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/testing/chainbase_fixture.hpp>
#include <eosio/testing/database_manager_fixture.hpp>
#include <eosio/testing/tester.hpp>

#include <boost/test/unit_test.hpp>

using namespace eosio::chain::resource_limits;
using namespace eosio::testing;
using namespace eosio::chain;
//TODO: subshard resource test.
class resource_limits_fixture: private database_manager_fixture<1024*1024>, public resource_limits_manager
{
   public:
      resource_limits_fixture()
      :database_manager_fixture()
      ,resource_limits_manager(*database_manager_fixture::_dbm, [](bool) { return nullptr; })
      {
         add_indices((*database_manager_fixture::_dbm).main_db());
         initialize_database();
      }

      ~resource_limits_fixture() {}

      chainbase::database::session start_session() {
         return database_manager_fixture::_dbm->main_db().start_undo_session(true);
      }

      chainbase::database& get_shard() const { return (*database_manager_fixture::_dbm).main_db();}
      chainbase::database& get_shared() const { return (*database_manager_fixture::_dbm).main_db();}

      void add_transaction_usage( const flat_set<account_name>& accounts, uint64_t cpu_usage, uint64_t net_usage, uint32_t ordinal,bool is_trx_transient = false ){
         chainbase::database&        db = get_shard();
         chainbase::database& shared_db = get_shared();
         resource_limits_manager::add_transaction_usage( accounts, cpu_usage, net_usage, ordinal, db, shared_db, is_trx_transient );
      }

      bool set_account_limits( const account_name& account, int64_t ram_bytes, int64_t net_weight, int64_t cpu_weight, bool is_trx_transient){
         chainbase::database& shared_db = get_shared();
         return resource_limits_manager::set_account_limits( account, ram_bytes, net_weight, cpu_weight, shared_db, is_trx_transient);
      }

      std::pair<int64_t, bool> get_account_cpu_limit( const account_name& name, uint32_t greylist_limit = config::maximum_elastic_resource_multiplier ) const{
               chainbase::database&        db = get_shard();
         const chainbase::database& shared_db = get_shared();
         resource_limits_manager::ensure_resource_limits_state_object( db, shared_db );
         return resource_limits_manager::get_account_cpu_limit( name, db, shared_db, greylist_limit );
      }

      std::pair<int64_t, bool> get_account_net_limit( const account_name& name, uint32_t greylist_limit = config::maximum_elastic_resource_multiplier ) const{
               chainbase::database&        db = get_shard();
         const chainbase::database& shared_db = get_shared();
         return resource_limits_manager::get_account_net_limit( name, db, shared_db, greylist_limit );
      }

      void add_pending_ram_usage( const account_name account, int64_t ram_delta, bool is_trx_transient = false ){
         chainbase::database&        db = get_shard();
         resource_limits_manager::add_pending_ram_usage( account, ram_delta, db, is_trx_transient );
      }

      void verify_account_ram_usage( const account_name accunt ) const {
         chainbase::database&        db = get_shard();
         chainbase::database& shared_db = get_shared();
         resource_limits_manager::verify_account_ram_usage( accunt, db, shared_db );
      }
};

constexpr uint64_t expected_elastic_iterations(uint64_t from, uint64_t to, uint64_t rate_num, uint64_t rate_den ) {
   uint64_t result = 0;
   uint64_t cur = from;

   while((from < to && cur < to) || (from > to && cur > to)) {
      cur = cur * rate_num / rate_den;
      result += 1;
   }

   return result;
}


constexpr uint64_t expected_exponential_average_iterations( uint64_t from, uint64_t to, uint64_t value, uint64_t window_size ) {
   uint64_t result = 0;
   uint64_t cur = from;

   while((from < to && cur < to) || (from > to && cur > to)) {
      cur = cur * (uint64_t)(window_size - 1) / (uint64_t)(window_size);
      cur += value / (uint64_t)(window_size);
      result += 1;
   }

   return result;
}

BOOST_AUTO_TEST_SUITE(resource_limits_test)

   /**
    * Test to make sure that the elastic limits for blocks relax and contract as expected
    */
   BOOST_FIXTURE_TEST_CASE(elastic_cpu_relax_contract, resource_limits_fixture) try {
      const uint64_t desired_virtual_limit = config::default_max_block_cpu_usage * config::maximum_elastic_resource_multiplier;
      const uint64_t expected_relax_iterations = expected_elastic_iterations( config::default_max_block_cpu_usage, desired_virtual_limit, 1000, 999 );

      // this is enough iterations for the average to reach/exceed the target (triggering congestion handling) and then the iterations to contract down to the min
      // subtracting 1 for the iteration that pulls double duty as reaching/exceeding the target and starting congestion handling
      const uint64_t expected_contract_iterations =
              expected_exponential_average_iterations(0, EOS_PERCENT(config::default_max_block_cpu_usage, config::default_target_block_cpu_usage_pct), config::default_max_block_cpu_usage, config::block_cpu_usage_average_window_ms / config::block_interval_ms ) +
              expected_elastic_iterations( desired_virtual_limit, config::default_max_block_cpu_usage, 99, 100 ) - 1;

      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, -1, -1, -1, false);
      process_account_limit_updates();

      // relax from the starting state (congested) to the idle state as fast as possible
      uint32_t iterations = 0;
      while( get_virtual_block_cpu_limit() < desired_virtual_limit && iterations <= expected_relax_iterations ) {
         add_transaction_usage({account},0,0,iterations);
         process_block_usage(iterations++);
      }

      BOOST_REQUIRE_EQUAL(iterations, expected_relax_iterations);
      BOOST_REQUIRE_EQUAL(get_virtual_block_cpu_limit(), desired_virtual_limit);

      // push maximum resources to go from idle back to congested as fast as possible
      while( get_virtual_block_cpu_limit() > config::default_max_block_cpu_usage
              && iterations <= expected_relax_iterations + expected_contract_iterations ) {
         add_transaction_usage({account}, config::default_max_block_cpu_usage, 0, iterations );
         process_block_usage(iterations++);
      }

      BOOST_REQUIRE_EQUAL(iterations, expected_relax_iterations + expected_contract_iterations);
      BOOST_REQUIRE_EQUAL(get_virtual_block_cpu_limit(), config::default_max_block_cpu_usage);
   } FC_LOG_AND_RETHROW();

   /**
    * Test to make sure that the elastic limits for blocks relax and contract as expected
    */
   BOOST_FIXTURE_TEST_CASE(elastic_net_relax_contract, resource_limits_fixture) try {
      const uint64_t desired_virtual_limit = config::default_max_block_net_usage * config::maximum_elastic_resource_multiplier;
      const uint64_t expected_relax_iterations = expected_elastic_iterations( config::default_max_block_net_usage, desired_virtual_limit, 1000, 999 );

      // this is enough iterations for the average to reach/exceed the target (triggering congestion handling) and then the iterations to contract down to the min
      // subtracting 1 for the iteration that pulls double duty as reaching/exceeding the target and starting congestion handling
      const uint64_t expected_contract_iterations =
              expected_exponential_average_iterations(0, EOS_PERCENT(config::default_max_block_net_usage, config::default_target_block_net_usage_pct), config::default_max_block_net_usage, config::block_size_average_window_ms / config::block_interval_ms ) +
              expected_elastic_iterations( desired_virtual_limit, config::default_max_block_net_usage, 99, 100 ) - 1;

      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, -1, -1, -1, false);
      process_account_limit_updates();

      // relax from the starting state (congested) to the idle state as fast as possible
      uint32_t iterations = 0;
      while( get_virtual_block_net_limit() < desired_virtual_limit && iterations <= expected_relax_iterations ) {
         add_transaction_usage({account},0,0,iterations );
         process_block_usage(iterations++);
      }

      BOOST_REQUIRE_EQUAL(iterations, expected_relax_iterations);
      BOOST_REQUIRE_EQUAL(get_virtual_block_net_limit(), desired_virtual_limit);

      // push maximum resources to go from idle back to congested as fast as possible
      while( get_virtual_block_net_limit() > config::default_max_block_net_usage
              && iterations <= expected_relax_iterations + expected_contract_iterations ) {
         add_transaction_usage({account},0, config::default_max_block_net_usage, iterations );
         process_block_usage(iterations++);
         init_block_pending_net(); //reset, mock producing block
      }

      BOOST_REQUIRE_EQUAL(iterations, expected_relax_iterations + expected_contract_iterations);
      BOOST_REQUIRE_EQUAL(get_virtual_block_net_limit(), config::default_max_block_net_usage);
   } FC_LOG_AND_RETHROW();

   /**
    * create 5 accounts with different weights, verify that the capacities are as expected and that usage properly enforces them
    */



   BOOST_FIXTURE_TEST_CASE(weighted_capacity_cpu, resource_limits_fixture) try {
      const vector<int64_t> weights = { 234, 511, 672, 800, 1213 };
      const int64_t total = std::accumulate(std::begin(weights), std::end(weights), 0LL);//3430
      vector<int64_t> expected_limits;//{}
      uint128_t account_cpu_usage_average_window = 24*60*60;
      uint128_t virtual_cpu_capacity_in_window = account_cpu_usage_average_window * (config::default_max_block_cpu_usage);
      std::transform(std::begin(weights), std::end(weights), std::back_inserter(expected_limits), [total, &virtual_cpu_capacity_in_window](const auto& v){ return (int64_t)(v * virtual_cpu_capacity_in_window / total); });

      for (int64_t idx = 0; idx < weights.size(); idx++) {
         const account_name account(idx + 100);
         initialize_account(account, false);
         set_account_limits(account, -1, -1, weights.at(idx), false);
      }

      process_account_limit_updates();//resource_limit_state_obj, total_cpu_weight = 3430
      int64_t average_window = 24*60*60;
      for (int64_t idx = 0; idx < weights.size(); idx++) {
         const account_name account(idx + 100);
         BOOST_CHECK_EQUAL(get_account_cpu_limit(account).first, expected_limits.at(idx));

         {  // use the expected limit, should succeed ... roll it back
            auto s = start_session();
            add_transaction_usage({account}, expected_limits.at(idx)/average_window, 0, 0 );
            s.undo();
         }

         // use too much, and expect failure; 2357737609
         BOOST_REQUIRE_THROW(add_transaction_usage({account}, expected_limits.at(idx) + 1, 0, 0 ), tx_cpu_usage_exceeded);
      }
   } FC_LOG_AND_RETHROW();

   /**
    * create 5 accounts with different weights, verify that the capacities are as expected and that usage properly enforces them
    */
   BOOST_FIXTURE_TEST_CASE(weighted_capacity_net, resource_limits_fixture) try {
      const vector<int64_t> weights = { 234, 511, 672, 800, 1213 };
      const int64_t total = std::accumulate(std::begin(weights), std::end(weights), 0LL);
      vector<int64_t> expected_limits;
      uint128_t account_net_usage_average_window = 24*60*60;
      uint128_t virtual_net_capacity_in_window = account_net_usage_average_window * (config::default_max_block_net_usage);
      std::transform(std::begin(weights), std::end(weights), std::back_inserter(expected_limits), [total, &virtual_net_capacity_in_window](const auto& v){ return v * virtual_net_capacity_in_window / total; });

      for (int64_t idx = 0; idx < weights.size(); idx++) {
         const account_name account(idx + 100);
         initialize_account(account, false);
         set_account_limits(account, -1, weights.at(idx), -1, false );
      }

      process_account_limit_updates();
      int64_t average_window = 24*60*60;
      for (int64_t idx = 0; idx < weights.size(); idx++) {
         const account_name account(idx + 100);
         BOOST_CHECK_EQUAL(get_account_net_limit(account).first, expected_limits.at(idx));

         {  // use the expected limit, should succeed ... roll it back
            auto s = start_session();
            add_transaction_usage({account}, 0, expected_limits.at(idx)/average_window, 0 );
            s.undo();
         }

         // use too much, and expect failure;
         BOOST_REQUIRE_THROW(add_transaction_usage({account}, 0, expected_limits.at(idx) + 1, 0 ), tx_net_usage_exceeded);
      }
   } FC_LOG_AND_RETHROW();


   BOOST_FIXTURE_TEST_CASE(enforce_block_limits_cpu, resource_limits_fixture) try {
      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, -1, -1, -1, false);
      process_account_limit_updates();

      const uint64_t increment = 1000;
      const uint64_t expected_iterations = config::default_max_block_cpu_usage / increment;

      for (uint64_t idx = 0; idx < expected_iterations; idx++) {
         add_transaction_usage({account}, increment, 0, 0 );
      }

      BOOST_REQUIRE_THROW(add_transaction_usage({account}, increment, 0, 0 ), block_resource_exhausted );

   } FC_LOG_AND_RETHROW();

   BOOST_FIXTURE_TEST_CASE(enforce_block_limits_net, resource_limits_fixture) try {
      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, -1, -1, -1, false);
      process_account_limit_updates();

      const uint64_t increment = 1000;
      const uint64_t expected_iterations = config::default_max_block_net_usage / increment;

      for (uint64_t idx = 0; idx < expected_iterations; idx++) {
         add_transaction_usage({account}, 0, increment, 0 );
      }

      BOOST_REQUIRE_THROW(add_transaction_usage({account}, 0, increment, 0 ), block_resource_exhausted );

   } FC_LOG_AND_RETHROW();

   BOOST_FIXTURE_TEST_CASE(enforce_account_ram_limit, resource_limits_fixture) try {
      const uint64_t limit = 1000;
      const uint64_t increment = 77;
      const uint64_t expected_iterations = (limit + increment - 1 ) / increment;

      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, limit, -1, -1, false);
      process_account_limit_updates();

      for (uint64_t idx = 0; idx < expected_iterations - 1; idx++) {
         add_pending_ram_usage(account, increment);
         verify_account_ram_usage(account);
      }

      add_pending_ram_usage(account, increment);
      BOOST_REQUIRE_THROW(verify_account_ram_usage(account), ram_usage_exceeded);
   } FC_LOG_AND_RETHROW();

   BOOST_FIXTURE_TEST_CASE(enforce_account_ram_limit_underflow, resource_limits_fixture) try {
      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, 100, -1, -1, false);
      verify_account_ram_usage(account);
      process_account_limit_updates();
      BOOST_REQUIRE_THROW(add_pending_ram_usage(account, -101), transaction_exception);

   } FC_LOG_AND_RETHROW();

   BOOST_FIXTURE_TEST_CASE(enforce_account_ram_limit_overflow, resource_limits_fixture) try {
      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, UINT64_MAX, -1, -1, false);
      verify_account_ram_usage(account);
      process_account_limit_updates();
      add_pending_ram_usage(account, UINT64_MAX/2 );
      verify_account_ram_usage(account);
      add_pending_ram_usage(account, UINT64_MAX/2);
      verify_account_ram_usage(account);
      BOOST_REQUIRE_THROW(add_pending_ram_usage(account, 2), transaction_exception);

   } FC_LOG_AND_RETHROW();

   BOOST_FIXTURE_TEST_CASE(enforce_account_ram_commitment, resource_limits_fixture) try {
      const int64_t limit = 1000;
      const int64_t commit = 600;
      const int64_t increment = 77;
      const int64_t expected_iterations = (limit - commit + increment - 1 ) / increment;
      const account_name account(1);
      initialize_account(account, false);
      set_account_limits(account, limit, -1, -1, false);
      process_account_limit_updates();
      add_pending_ram_usage(account, commit);
      verify_account_ram_usage(account);

      for (int idx = 0; idx < expected_iterations - 1; idx++) {
         set_account_limits(account, limit - increment * idx, -1, -1, false);
         verify_account_ram_usage(account);
         process_account_limit_updates();
      }

      set_account_limits(account, limit - increment * expected_iterations, -1, -1, false);
      BOOST_REQUIRE_THROW(verify_account_ram_usage(account), ram_usage_exceeded);
   } FC_LOG_AND_RETHROW();


   BOOST_FIXTURE_TEST_CASE(sanity_check, resource_limits_fixture) try {
      int64_t  total_staked_tokens = 1'000'000'000'0000ll;
      int64_t  user_stake = 1'0000ll;
      uint64_t max_block_cpu = 200'000.; // us;
      uint64_t blocks_per_day = calc_blocks_by_sec(60*60*24);
      uint64_t total_cpu_per_period = max_block_cpu * blocks_per_day;
      double congested_cpu_time_per_period = (double(total_cpu_per_period) * user_stake) / total_staked_tokens;
      wdump((congested_cpu_time_per_period));
      double uncongested_cpu_time_per_period = congested_cpu_time_per_period * config::maximum_elastic_resource_multiplier;
      wdump((uncongested_cpu_time_per_period));

      initialize_account( "dan"_n, false );
      initialize_account( "everyone"_n, false );
      set_account_limits( "dan"_n, 0, 0, user_stake, false );
      set_account_limits( "everyone"_n, 0, 0, (total_staked_tokens - user_stake), false );
      process_account_limit_updates();

      // dan cannot consume more than 34 us per day
      BOOST_REQUIRE_THROW( add_transaction_usage( {"dan"_n}, 35, 0, 1 ), tx_cpu_usage_exceeded );

      // Ensure CPU usage is 0 by "waiting" for one day's worth of blocks to pass.
      add_transaction_usage( {"dan"_n}, 0, 0, 1 + blocks_per_day );

      // But dan should be able to consume up to 34 us per day.
      add_transaction_usage( {"dan"_n}, 34, 0, 2 + blocks_per_day );
   } FC_LOG_AND_RETHROW()

   /**
     * Test to make sure that get_account_net_limit_ex and get_account_cpu_limit_ex returns proper results, including
     * 1. the last updated timestamp is always same as the time slot on accumulator.
     * 2. when no timestamp is given, the current_used should be same as used.
     * 3. when timestamp is given, if it is earlier than last_usage_update_time, current_used is same as used,
     *    otherwise, current_used should be the decay value (will be 0 after 1 day)
    */
   BOOST_FIXTURE_TEST_CASE(get_account_net_limit_ex_and_get_account_cpu_limit_ex, resource_limits_fixture) try {

      const account_name cpu_test_account("cpuacc");
      const account_name net_test_account("netacc");
      constexpr uint32_t net_window = eosio::chain::config::account_net_usage_average_window_ms / eosio::chain::config::block_interval_ms;
      constexpr uint32_t cpu_window = eosio::chain::config::account_cpu_usage_average_window_ms / eosio::chain::config::block_interval_ms;

      constexpr int64_t unlimited = -1;

      using get_account_limit_ex_func = std::function<std::pair<account_resource_limit, bool>(const resource_limits_manager*, const account_name&, chainbase::database& shard_db, const chainbase::database& shared_db, uint32_t, const std::optional<block_timestamp_type>&)>;
      auto test_get_account_limit_ex = [this](const account_name& test_account, const uint32_t window, get_account_limit_ex_func get_account_limit_ex)
      {
         chainbase::database& shard_db = get_shard();
         chainbase::database& shared_db = get_shared();
         constexpr uint32_t delta_slot = 100;
         constexpr uint32_t greylist_limit = config::maximum_elastic_resource_multiplier;
         const block_timestamp_type time_stamp_now(delta_slot + 1);
         BOOST_CHECK_LT(delta_slot, window);
         initialize_account(test_account, false);
         set_account_limits(test_account, unlimited, unlimited, unlimited, false);
         process_account_limit_updates();
         // unlimited
         {
            const auto ret_unlimited_wo_time_stamp = get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, {});
            const auto ret_unlimited_with_time_stamp = get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, time_stamp_now);
            BOOST_CHECK_EQUAL(ret_unlimited_wo_time_stamp.first.current_used, (int64_t) -1);
            BOOST_CHECK_EQUAL(ret_unlimited_with_time_stamp.first.current_used, (int64_t) -1);
            BOOST_CHECK_EQUAL(ret_unlimited_wo_time_stamp.first.last_usage_update_time.slot,
                              ret_unlimited_with_time_stamp.first.last_usage_update_time.slot);

         }
         const int64_t cpu_limit = 2048;
         const int64_t net_limit = 1024;
         set_account_limits(test_account, -1, net_limit, cpu_limit, false);
         process_account_limit_updates();
         // limited, with no usage, current time stamp
         {
            const auto ret_limited_init_wo_time_stamp =  get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, {});
            const auto ret_limited_init_with_time_stamp =  get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, time_stamp_now);
            BOOST_CHECK_EQUAL(ret_limited_init_wo_time_stamp.first.current_used, ret_limited_init_wo_time_stamp.first.used);
            BOOST_CHECK_EQUAL(ret_limited_init_wo_time_stamp.first.current_used, 0);
            BOOST_CHECK_EQUAL(ret_limited_init_with_time_stamp.first.current_used, ret_limited_init_with_time_stamp.first.used);
            BOOST_CHECK_EQUAL(ret_limited_init_with_time_stamp.first.current_used, 0);
            BOOST_CHECK_EQUAL(ret_limited_init_wo_time_stamp.first.last_usage_update_time.slot, 0 );
            BOOST_CHECK_EQUAL( ret_limited_init_with_time_stamp.first.last_usage_update_time.slot, 0 );
         }
         const uint32_t update_slot = time_stamp_now.slot - delta_slot;
         const int64_t cpu_usage = 100;
         const int64_t net_usage = 200;
         add_transaction_usage({test_account}, cpu_usage, net_usage, update_slot );
         // limited, with some usages, current time stamp
         {
            const auto ret_limited_1st_usg_wo_time_stamp =  get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, {});
            const auto ret_limited_1st_usg_with_time_stamp =  get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, time_stamp_now);
            BOOST_CHECK_EQUAL(ret_limited_1st_usg_wo_time_stamp.first.current_used, ret_limited_1st_usg_wo_time_stamp.first.used);
            BOOST_CHECK_LT(ret_limited_1st_usg_with_time_stamp.first.current_used, ret_limited_1st_usg_with_time_stamp.first.used);
            BOOST_CHECK_EQUAL(ret_limited_1st_usg_with_time_stamp.first.current_used,
                              ret_limited_1st_usg_with_time_stamp.first.used * (window - delta_slot) / window);
            BOOST_CHECK_EQUAL(ret_limited_1st_usg_wo_time_stamp.first.last_usage_update_time.slot, update_slot);
            BOOST_CHECK_EQUAL(ret_limited_1st_usg_with_time_stamp.first.last_usage_update_time.slot, update_slot);
         }
         // limited, with some usages, earlier time stamp
         const block_timestamp_type earlier_time_stamp(time_stamp_now.slot - delta_slot - 1);
         {
            const auto ret_limited_wo_time_stamp =  get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, {});
            const auto ret_limited_with_earlier_time_stamp =  get_account_limit_ex(this, test_account, shard_db, shared_db, greylist_limit, earlier_time_stamp);
            BOOST_CHECK_EQUAL(ret_limited_with_earlier_time_stamp.first.current_used, ret_limited_with_earlier_time_stamp.first.used);
            BOOST_CHECK_EQUAL(ret_limited_wo_time_stamp.first.current_used, ret_limited_wo_time_stamp.first.used );
            BOOST_CHECK_EQUAL(ret_limited_wo_time_stamp.first.last_usage_update_time.slot, update_slot);
            BOOST_CHECK_EQUAL(ret_limited_with_earlier_time_stamp.first.last_usage_update_time.slot, update_slot);
         }
      };
      test_get_account_limit_ex(net_test_account, net_window, &resource_limits_manager::get_account_net_limit_ex);
      test_get_account_limit_ex(cpu_test_account, cpu_window, &resource_limits_manager::get_account_cpu_limit_ex);

   } FC_LOG_AND_RETHROW()


   BOOST_AUTO_TEST_SUITE_END()
