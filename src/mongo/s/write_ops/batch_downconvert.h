/**
 *    Copyright (C) 2013 MongoDB Inc.
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

#include <string>
#include <vector>

#include "mongo/base/string_data.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/optime.h"
#include "mongo/client/dbclientinterface.h"
#include "mongo/s/multi_command_dispatch.h"
#include "mongo/s/write_ops/batch_write_exec.h"
#include "mongo/s/write_ops/batch_write_op.h"
#include "mongo/s/write_ops/batched_command_request.h"
#include "mongo/s/write_ops/batched_command_response.h"
#include "mongo/s/write_ops/write_error_detail.h"

// TODO: Remove post-2.6

namespace mongo {

    /**
     * Interface to execute a single safe write and enforce write concern on a connection.
     */
    class SafeWriter {
    public:

        virtual ~SafeWriter() {
        }

        /**
         * Sends a write to a remote host and returns a GLE response.
         */
        virtual Status safeWrite( DBClientBase* conn,
                                  const BatchItemRef& batchItem,
                                  const BSONObj& writeConcern,
                                  BSONObj* gleResponse ) = 0;

        /**
         * Purely enforces a write concern on a remote host by clearing the previous error.
         * This is more expensive than a normal safe write, but is sometimes needed to support
         * write command emulation.
         */
        virtual Status enforceWriteConcern( DBClientBase* conn,
                                            const StringData& dbName,
                                            const BSONObj& writeConcern,
                                            BSONObj* gleResponse ) = 0;
    };

    /**
     * Executes a batch write using safe writes.
     *
     * The actual safe write operation is done via an interface to allow testing the rest of the
     * aggregation functionality.
     */
    class BatchSafeWriter {
    public:

        BatchSafeWriter( SafeWriter* safeWriter ) :
            _safeWriter( safeWriter ) {
        }

        // Testable static dispatching method, defers to SafeWriter for actual writes over the
        // connection.
        void safeWriteBatch( DBClientBase* conn,
                             const BatchedCommandRequest& request,
                             BatchedCommandResponse* response );

        // Helper that acts as an auto-ptr for write and wc errors
        struct GLEErrors {
            auto_ptr<WriteErrorDetail> writeError;
            auto_ptr<WCErrorDetail> wcError;
        };

        /**
         * Given a GLE response, extracts a write error and a write concern error for the previous
         * operation.
         *
         * Returns !OK if the GLE itself failed in an unknown way.
         */
        static Status extractGLEErrors( const BSONObj& gleResponse, GLEErrors* errors );

        struct GLEStats {
            GLEStats() :
                n( 0 ) {
            }

            int n;
            BSONObj upsertedId;
            OpTime lastOp;
        };

        /**
         * Given a GLE response, pulls out stats for the previous write operation.
         */
        static void extractGLEStats( const BSONObj& gleResponse, GLEStats* stats );

        /**
         * Given a GLE response, strips out all non-write-concern related information
         */
        static BSONObj stripNonWCInfo( const BSONObj& gleResponse );

    private:

        SafeWriter* _safeWriter;
    };

    // Used for reporting legacy write concern responses
    struct LegacyWCResponse {
        string shardHost;
        BSONObj gleResponse;
        string errToReport;
    };

    /**
     * Uses GLE and the shard hosts and opTimes last written by write commands to enforce a
     * write concern across the previously used shards.
     *
     * Returns OK with the LegacyWCResponses containing only write concern error information
     * Returns !OK if there was an error getting a GLE response
     */
    Status enforceLegacyWriteConcern( MultiCommandDispatch* dispatcher,
                                      const StringData& dbName,
                                      const BSONObj& options,
                                      const HostOpTimeMap& hostOpTimes,
                                      vector<LegacyWCResponse>* wcResponses );
}
