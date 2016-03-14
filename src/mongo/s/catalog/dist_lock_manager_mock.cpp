/**
 *    Copyright (C) 2015 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kSharding

#include "mongo/platform/basic.h"

#include "mongo/s/catalog/dist_lock_manager_mock.h"

#include <algorithm>

#include "mongo/util/mongoutils/str.h"

namespace mongo {

    using stdx::chrono::milliseconds;

    DistLockManagerMock::DistLockManagerMock() : _lockReturnStatus{Status::OK()}{}

    void DistLockManagerMock::startUp() {}

    void DistLockManagerMock::shutDown() {
        uassert(28659,
                "DistLockManagerMock shut down with outstanding locks present",
                _locks.empty());
    }

    StatusWith<DistLockManager::ScopedDistLock> DistLockManagerMock::lock(
            StringData name,
            StringData whyMessage,
            milliseconds waitFor,
            milliseconds lockTryInterval) {

        if (!_lockReturnStatus.isOK()) {
            return _lockReturnStatus;
        }

        if (_locks.end() != std::find_if(
                _locks.begin(),
                _locks.end(),
                [name](LockInfo info)->bool { return info.name == name; })) {
            return Status(ErrorCodes::LockBusy,
                          str::stream() << "Lock \"" << name << "\" is already taken");
        }

        LockInfo info;
        info.name = name.toString();
        info.lockID = DistLockHandle::gen();
        _locks.push_back(info);

        return DistLockManager::ScopedDistLock(info.lockID, this);
    }

    void DistLockManagerMock::unlock(const DistLockHandle& lockHandle) {
        std::vector<LockInfo>::iterator it =
                std::find_if(
                        _locks.begin(),
                        _locks.end(),
                        [&lockHandle](LockInfo info)->bool { return info.lockID == lockHandle; });
        if (it == _locks.end()) {
            return;
        }
        _locks.erase(it);
    }

    Status DistLockManagerMock::checkStatus(const DistLockHandle& lockHandle) {
        return Status::OK();
    }

    void DistLockManagerMock::setLockReturnStatus(Status status) {
        _lockReturnStatus = std::move(status);
    }

}
