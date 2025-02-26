/*
 * Copyright (2022) Bytedance Ltd. and/or its affiliates
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <Storages/MergeTree/DeleteBitmapMeta.h>
#include <Storages/MergeTree/IMergeTreeDataPart_fwd.h>
#include <Storages/MergeTree/MergeTreeDataPartCNCH_fwd.h>
#include <Transaction/TxnTimestamp.h>
#include <WorkerTasks/ManipulationType.h>
#include <Transaction/Actions/S3AttachMetaAction.h>
#include <cppkafka/cppkafka.h>
#include <cppkafka/topic_partition_list.h>
#include <Databases/MySQL/MaterializedMySQLCommon.h>

namespace DB
{
class MergeTreeMetaBase;

struct DumpedData
{
    MutableMergeTreeDataPartsCNCHVector parts;
    DeleteBitmapMetaPtrVector bitmaps;
    MutableMergeTreeDataPartsCNCHVector staged_parts;

    void extend(DumpedData && data);
};

// TODO: proper writer
class CnchDataWriter : private boost::noncopyable
{
public:
    CnchDataWriter(
        MergeTreeMetaBase & storage_,
        ContextPtr context_,
        ManipulationType type_,
        String task_id_ = {},
        String consumer_group_ = {},
        const cppkafka::TopicPartitionList & tpl_ = {},
        const MySQLBinLogInfo & binlog = {});

    ~CnchDataWriter();

    DumpedData dumpAndCommitCnchParts(
        const IMutableMergeTreeDataPartsVector & temp_parts,
        const LocalDeleteBitmaps & temp_bitmaps = {},
        const IMutableMergeTreeDataPartsVector & temp_staged_parts = {});

    void initialize(size_t max_threads);

    void schedule(
        const IMutableMergeTreeDataPartsVector & temp_parts,
        const LocalDeleteBitmaps & temp_bitmaps,
        const IMutableMergeTreeDataPartsVector & temp_staged_parts = {});

    void finalize();

    // server side only
    void commitPreparedCnchParts(const DumpedData & data, const std::unique_ptr<S3AttachPartsInfo>& s3_parts_info = nullptr);


    /// Convert staged parts to visible parts along with the given delete bitmaps.
    void publishStagedParts(const MergeTreeDataPartsCNCHVector & staged_parts, const LocalDeleteBitmaps & bitmaps_to_dump);

    DumpedData dumpCnchParts(
        const IMutableMergeTreeDataPartsVector & temp_parts,
        const LocalDeleteBitmaps & temp_bitmaps = {},
        const IMutableMergeTreeDataPartsVector & temp_staged_parts = {});

    void commitDumpedParts(const DumpedData & dumped_data);

    void preload(const MutableMergeTreeDataPartsCNCHVector & dumped_parts);

    DumpedData res;

private:
    MergeTreeMetaBase & storage;
    ContextPtr context;
    ManipulationType type;
    String task_id;

    String consumer_group;
    cppkafka::TopicPartitionList tpl;
    MySQLBinLogInfo binlog;

    UUID newPartID(const MergeTreePartInfo & part_info, UInt64 txn_timestamp);
    /// dump with thread pool
    std::unique_ptr<ThreadPool> thread_pool;
    ExceptionHandler handler;
    std::atomic_bool cancelled{false};
    mutable std::mutex write_mutex;
};

}
