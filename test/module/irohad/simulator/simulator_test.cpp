/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "backend/protobuf/transaction.hpp"
#include "module/irohad/ametsuchi/ametsuchi_mocks.hpp"
#include "module/irohad/model/model_mocks.hpp"
#include "module/irohad/network/network_mocks.hpp"
#include "module/irohad/validation/validation_mocks.hpp"
#include "module/shared_model/builders/protobuf/test_block_builder.hpp"
#include "module/shared_model/builders/protobuf/test_proposal_builder.hpp"

#include "framework/test_subscriber.hpp"
#include "simulator/impl/simulator.hpp"

using namespace iroha;
using namespace iroha::validation;
using namespace iroha::ametsuchi;
using namespace iroha::model;
using namespace iroha::simulator;
using namespace iroha::network;
using namespace framework::test_subscriber;

using ::testing::A;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::_;

using wBlock = std::shared_ptr<shared_model::interface::Block>;

class SimulatorTest : public ::testing::Test {
 public:
  void SetUp() override {
    validator = std::make_shared<MockStatefulValidator>();
    factory = std::make_shared<MockTemporaryFactory>();
    query = std::make_shared<MockBlockQuery>();
    ordering_gate = std::make_shared<MockOrderingGate>();
    crypto_provider = std::make_shared<MockCryptoProvider>();

    empty_proposal = std::make_shared<shared_model::proto::Proposal>(
        TestProposalBuilder().build());
  }

  void init() {
    simulator = std::make_shared<Simulator>(
        ordering_gate, validator, factory, query, crypto_provider);
  }

  std::shared_ptr<MockStatefulValidator> validator;
  std::shared_ptr<MockTemporaryFactory> factory;
  std::shared_ptr<MockBlockQuery> query;
  std::shared_ptr<MockOrderingGate> ordering_gate;
  std::shared_ptr<MockCryptoProvider> crypto_provider;

  std::shared_ptr<shared_model::interface::Proposal> empty_proposal;

  std::shared_ptr<Simulator> simulator;
};

shared_model::proto::Block makeBlock(int height) {
  return TestBlockBuilder()
      .transactions(std::vector<shared_model::proto::Transaction>())
      .height(height)
      .prevHash(shared_model::crypto::Hash(std::string("0", 32)))
      .build();
}

shared_model::proto::Proposal makeProposal(int height) {
  return TestProposalBuilder()
      .transactions(std::vector<shared_model::proto::Transaction>())
      .height(height)
      .build();
}

TEST_F(SimulatorTest, ValidWhenInitialized) {
  // simulator constructor => on_proposal subscription called
  EXPECT_CALL(*ordering_gate, on_proposal())
      .WillOnce(Return(rxcpp::observable<>::empty<std::shared_ptr<shared_model::interface::Proposal>>()));
                       //just(empty_proposal)));

  init();
}

TEST_F(SimulatorTest, ValidWhenPreviousBlock) {
  // proposal with height 2 => height 1 block present => new block generated
  auto proposal =
      std::make_shared<shared_model::proto::Proposal>(makeProposal(2));

  shared_model::proto::Block block = makeBlock(proposal->height() - 1);

  EXPECT_CALL(*factory, createTemporaryWsv()).Times(1);
  EXPECT_CALL(*query, getTopBlocks(1))
      .WillOnce(Return(rxcpp::observable<>::just(block).map(
          [](auto &&x) { return wBlock(x.copy()); })));

  EXPECT_CALL(*validator, validate(_, _)).WillOnce(Return(proposal));

  EXPECT_CALL(*ordering_gate, on_proposal())
      .WillOnce(Return(rxcpp::observable<>::empty<std::shared_ptr<shared_model::interface::Proposal>>()));
//      .WillOnce(Return(rxcpp::observable<>::just(empty_proposal)));

  EXPECT_CALL(*crypto_provider, sign(A<model::Block &>())).Times(1);

  init();

  auto proposal_wrapper =
      make_test_subscriber<CallExact>(simulator->on_verified_proposal(), 1);
  proposal_wrapper.subscribe([&proposal](auto verified_proposal) {
    ASSERT_EQ(verified_proposal->height(), proposal->height());
    ASSERT_EQ(verified_proposal->transactions(), proposal->transactions());
  });

  auto block_wrapper =
      make_test_subscriber<CallExact>(simulator->on_block(), 1);
  block_wrapper.subscribe([&proposal](auto block) {
    ASSERT_EQ(block->height(), proposal->height());
    ASSERT_EQ(block->transactions(), proposal->transactions());
  });

  simulator->process_proposal(*proposal);

  ASSERT_TRUE(proposal_wrapper.validate());
  ASSERT_TRUE(block_wrapper.validate());
}

TEST_F(SimulatorTest, FailWhenNoBlock) {
  // height 2 proposal => height 1 block not present => no validated proposal
  auto proposal = makeProposal(2);

  EXPECT_CALL(*factory, createTemporaryWsv()).Times(0);

  EXPECT_CALL(*query, getTopBlocks(1))
      .WillOnce(Return(rxcpp::observable<>::empty<wBlock>()));

  EXPECT_CALL(*validator, validate(_, _)).Times(0);

  EXPECT_CALL(*ordering_gate, on_proposal())
      .WillOnce(Return(rxcpp::observable<>::empty<std::shared_ptr<shared_model::interface::Proposal>>()));
//  .WillOnce(Return(rxcpp::observable<>::just(empty_proposal)));

  EXPECT_CALL(*crypto_provider, sign(A<model::Block &>())).Times(0);

  init();

  auto proposal_wrapper =
      make_test_subscriber<CallExact>(simulator->on_verified_proposal(), 0);
  proposal_wrapper.subscribe();

  auto block_wrapper =
      make_test_subscriber<CallExact>(simulator->on_block(), 0);
  block_wrapper.subscribe();

  simulator->process_proposal(proposal);

  ASSERT_TRUE(proposal_wrapper.validate());
  ASSERT_TRUE(block_wrapper.validate());
}

TEST_F(SimulatorTest, FailWhenSameAsProposalHeight) {
  // proposal with height 2 => height 2 block present => no validated proposal
  auto proposal = makeProposal(2);

  auto block = makeBlock(proposal.height());

  EXPECT_CALL(*factory, createTemporaryWsv()).Times(0);

  EXPECT_CALL(*query, getTopBlocks(1))
      .WillOnce(Return(rxcpp::observable<>::just(block).map(
          [](auto &&x) { return wBlock(x.copy()); })));

  EXPECT_CALL(*validator, validate(_, _)).Times(0);

  EXPECT_CALL(*ordering_gate, on_proposal())
      .WillOnce(Return(rxcpp::observable<>::empty<std::shared_ptr<shared_model::interface::Proposal>>()));
//  .WillOnce(Return(rxcpp::observable<>::just(empty_proposal)));

  EXPECT_CALL(*crypto_provider, sign(A<model::Block &>())).Times(0);

  init();

  auto proposal_wrapper =
      make_test_subscriber<CallExact>(simulator->on_verified_proposal(), 0);
  proposal_wrapper.subscribe();

  auto block_wrapper =
      make_test_subscriber<CallExact>(simulator->on_block(), 0);
  block_wrapper.subscribe();

  simulator->process_proposal(proposal);

  ASSERT_TRUE(proposal_wrapper.validate());
  ASSERT_TRUE(block_wrapper.validate());
}
