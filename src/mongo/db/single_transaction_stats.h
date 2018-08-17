/**
 *    Copyright (C) 2018 MongoDB Inc.
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

#pragma once

#include "mongo/db/curop.h"
#include "mongo/rpc/metadata/client_metadata.h"
#include "mongo/rpc/metadata/client_metadata_ismaster.h"

namespace mongo {

/**
 * Tracks metrics for a single multi-document transaction.
 */
class SingleTransactionStats {
public:
    /**
     * Stores information about the last client to run a transaction operation.
     */
    struct LastClientInfo {
        std::string clientHostAndPort;
        long long connectionId;
        BSONObj clientMetadata;
        std::string appName;

        void update(Client* client) {
            if (client->hasRemote()) {
                clientHostAndPort = client->getRemote().toString();
            }
            connectionId = client->getConnectionId();
            if (const auto& metadata =
                    ClientMetadataIsMasterState::get(client).getClientMetadata()) {
                clientMetadata = metadata.get().getDocument();
                appName = metadata.get().getApplicationName().toString();
            }
        }
    };

    /**
     * Returns the start time of the transaction in microseconds.
     *
     * This method cannot be called until setStartTime() has been called.
     */
    unsigned long long getStartTime() const;

    /**
     * Sets the transaction's start time, only if it hasn't already been set.
     *
     * This method must only be called once.
     */
    void setStartTime(unsigned long long time);

    /**
     * If the transaction is currently in progress, this method returns the duration
     * the transaction has been running for in microseconds, given the current time value.
     *
     * For a completed transaction, this method returns the total duration of the
     * transaction in microseconds.
     *
     * This method cannot be called until setStartTime() has been called.
     */
    unsigned long long getDuration(unsigned long long curTime) const;

    /**
     * Sets the transaction's end time, only if the start time has already been set.
     *
     * This method cannot be called until setStartTime() has been called.
     */
    void setEndTime(unsigned long long time);

    /**
     * Returns the total active time of the transaction, given the current time value. A transaction
     * is active when there is a running operation that is part of the transaction.
     */
    Microseconds getTimeActiveMicros(unsigned long long curTime) const;

    /**
     * Returns the total inactive time of the transaction, given the current time value. A
     * transaction is inactive when it is idly waiting for a new operation to occur.
     */
    Microseconds getTimeInactiveMicros(unsigned long long curTime) const;

    /**
     * Marks the transaction as active and sets the start of the transaction's active time.
     *
     * This method cannot be called if the transaction is currently active. A call to setActive()
     * must be followed by a call to setInactive() before calling setActive() again.
     */
    void setActive(unsigned long long time);

    /**
     * Marks the transaction as inactive and sets the total active time of the transaction. The
     * total active time will only be set if the transaction was active prior to this call.
     *
     * This method cannot be called if the transaction is currently not active.
     */
    void setInactive(unsigned long long time);

    /**
     * Returns whether or not the transaction is currently active.
     */
    bool isActive() const {
        return _lastTimeActiveStart != 0;
    }

    /**
     * Returns the OpDebug object stored in this SingleTransactionStats instance.
     */
    OpDebug* getOpDebug() {
        return &_opDebug;
    }

    /**
     * Returns the LastClientInfo object stored in this SingleTransactionStats instance.
     */
    LastClientInfo getLastClientInfo() const {
        return _lastClientInfo;
    }

    /**
     * Updates the LastClientInfo object stored in this SingleTransactionStats instance with the
     * given Client's information.
     */
    void updateLastClientInfo(Client* client) {
        _lastClientInfo.update(client);
    }

private:
    // The start time of the transaction in microseconds.
    unsigned long long _startTime{0};

    // The end time of the transaction in microseconds.
    unsigned long long _endTime{0};

    // The total amount of active time spent by the transaction.
    Microseconds _timeActiveMicros = Microseconds{0};

    // The time at which the transaction was last marked as active in microseconds. The transaction
    // is considered active if this value is not equal to 0.
    unsigned long long _lastTimeActiveStart{0};

    // Tracks and accumulates stats from all operations that run inside the transaction.
    OpDebug _opDebug;

    // Holds information about the last client to run a transaction operation.
    LastClientInfo _lastClientInfo;
};

}  // namespace mongo
