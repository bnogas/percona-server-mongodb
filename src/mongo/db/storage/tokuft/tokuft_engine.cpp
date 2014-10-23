/**
 *    Copyright (C) 2014 MongoDB Inc.
 *    Copyright (C) 2014 Tokutek Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage

#include "mongo/db/operation_context.h"
#include "mongo/db/storage/tokuft/tokuft_dictionary.h"
#include "mongo/db/storage/tokuft/tokuft_engine.h"
#include "mongo/db/storage/tokuft/tokuft_recovery_unit.h"
#include "mongo/util/log.h"

#include <ftcxx/db_env.hpp>
#include <ftcxx/db_env-inl.hpp>
#include <ftcxx/db.hpp>

namespace mongo {
    
    // TODO: Real error handling

    static const int env_mode = 0755;
    static const int env_flags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE |
        DB_PRIVATE | DB_INIT_LOG | DB_RECOVER;

    namespace {

        static int tokuft_bt_compare(const ftcxx::Slice &desc, const ftcxx::Slice &a, const ftcxx::Slice &b) {
            TokuFTDictionary::Comparator cmp(desc);
            return cmp(a, b);
        }

    }

    TokuFTEngine::TokuFTEngine(const std::string &path)
        : _env(nullptr),
          _metadataDict(nullptr)
    {
        log() << "tokuft-engine: opening environment at " << path << std::endl;
        _env = ftcxx::DBEnvBuilder()
            // TODO: Direct I/O
            // TODO: Lock wait timeout callback, lock killed callback,
            //       Fsync log period, redzone, logdir, etc
            // TODO: Checkpoint period, cleaner period
            .set_default_bt_compare(&ftcxx::wrapped_comparator<tokuft_bt_compare>)
            .open(path.c_str(), env_flags, env_mode);

        ftcxx::DBTxn txn(_env);
        _metadataDict.reset(new TokuFTDictionary(_env, txn, "tokuft.metadata", KVDictionary::Comparator::useMemcmp()));
        txn.commit();
    }

    TokuFTEngine::~TokuFTEngine() {
        invariant(_env.env() != NULL);

        log() << "tokuft-engine: shutdown" << std::endl;
    }

    static const ftcxx::DBTxn &_getDBTxn(OperationContext *opCtx) {
        TokuFTRecoveryUnit *ru = dynamic_cast<TokuFTRecoveryUnit *>(opCtx->recoveryUnit());
        invariant(ru != NULL);
        return ru->txn();
    }

    RecoveryUnit *TokuFTEngine::newRecoveryUnit() {
        return new TokuFTRecoveryUnit(_env);
    }

    Status TokuFTEngine::createKVDictionary( OperationContext* opCtx,
                                              const StringData& ident,
                                              const KVDictionary::Comparator &cmp ) {
        TokuFTDictionary dict(_env, _getDBTxn(opCtx), ident, cmp);
        invariant(dict.db().db() != NULL);

        return Status::OK();
    }

    KVDictionary* TokuFTEngine::getKVDictionary( OperationContext* opCtx,
                                                  const StringData& ident,
                                                  const KVDictionary::Comparator &cmp,
                                                  bool mayCreate ) {
        // TODO: mayCreate
        return new TokuFTDictionary(_env, _getDBTxn(opCtx), ident, cmp);
    }

    Status TokuFTEngine::dropKVDictionary( OperationContext* opCtx,
                                            const StringData& ident ) {
        invariant(ident.size() > 0);

        std::string identStr = ident.toString();
        const int r = _env.env()->dbremove(_env.env(), _getDBTxn(opCtx).txn(), identStr.c_str(), NULL, 0);
        invariant(r == 0);
        return Status::OK();
    }

} // namespace mongo
