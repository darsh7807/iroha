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

#ifndef IROHA_SHARED_MODEL_TRANSFER_ASSET_HPP
#define IROHA_SHARED_MODEL_TRANSFER_ASSET_HPP

#include "interfaces/common_objects/types.hpp"
#include "interfaces/hashable.hpp"
#include "model/commands/transfer_asset.hpp"

namespace shared_model {
  namespace interface {
    /**
     * Grant permission to account
     */
    class TransferAsset
        : public Hashable<TransferAsset, iroha::model::TransferAsset> {
     public:
      /**
       * @return Id of the account from which transfer assets
       */
      virtual const types::AccountIdType &srcAccountId() const = 0;
      /**
       * @return Id of the account to which transfer assets
       */
      virtual const types::AccountIdType &destAccountId() const = 0;
      /**
       * @return Id of the asset to transfer
       */
      virtual const types::AssetIdType &assetId() const = 0;
      /// Type of the transfer message
      using MessageType = std::string;
      /**
       * @return message of the transfer
       */
      virtual const MessageType &message() const = 0;
      /**
       * @return asset amount to transfer
       */
      virtual const types::AmountType &amount() const = 0;

      std::string toString() const override {
        return detail::PrettyStringBuilder()
            .init("TransferAsset")
            .append("src_account_id", srcAccountId())
            .append("dest_account_id", destAccountId())
            .append("asset_id", assetId())
            .append("message", message())
            .append("amount", amount().to_string())
            .finalize();
      }

      OldModelType *makeOldModel() const override {
        auto oldModel = new iroha::model::TransferAsset;
        oldModel->src_account_id = srcAccountId();
        oldModel->dest_account_id = destAccountId();
        oldModel->amount = amount();
        oldModel->asset_id = assetId();
        oldModel->description = message();
        return oldModel;
      }
    };
  }  // namespace interface
}  // namespace shared_model

#endif  // IROHA_SHARED_MODEL_TRANSFER_ASSET_HPP