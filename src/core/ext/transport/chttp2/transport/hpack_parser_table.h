/*
 *
 * Copyright 2015 gRPC authors.
 *
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
 *
 */

#ifndef GRPC_CORE_EXT_TRANSPORT_CHTTP2_TRANSPORT_HPACK_PARSER_TABLE_H
#define GRPC_CORE_EXT_TRANSPORT_CHTTP2_TRANSPORT_HPACK_PARSER_TABLE_H

#include <grpc/support/port_platform.h>

#include <grpc/slice.h>

#include "src/core/ext/transport/chttp2/transport/hpack_constants.h"
#include "src/core/lib/gprpp/memory.h"
#include "src/core/lib/iomgr/error.h"
#include "src/core/lib/transport/metadata_batch.h"

namespace grpc_core {

// HPACK header table
class HPackTable {
 public:
  HPackTable();
  ~HPackTable();

  HPackTable(const HPackTable&) = delete;
  HPackTable& operator=(const HPackTable&) = delete;

  void SetMaxBytes(uint32_t max_bytes);
  grpc_error_handle SetCurrentTableSize(uint32_t bytes);

  using Memento = ParsedMetadata<grpc_metadata_batch>;

  // Lookup, but don't ref.
  const Memento* Lookup(uint32_t index) const {
    // Static table comes first, just return an entry from it.
    // NB: This imposes the constraint that the first
    // GRPC_CHTTP2_LAST_STATIC_ENTRY entries in the core static metadata table
    // must follow the hpack standard. If that changes, we *must* not rely on
    // reading the core static metadata table here; at that point we'd need our
    // own singleton static metadata in the correct order.
    if (index <= hpack_constants::kLastStaticEntry) {
      return &static_metadata_.memento[index - 1];
    } else {
      return LookupDynamic(index);
    }
  }

  // add a table entry to the index
  grpc_error_handle Add(Memento md) GRPC_MUST_USE_RESULT;

  // Current entry count in the table.
  uint32_t num_entries() const { return entries_.num_entries(); }

 private:
  struct StaticMementos {
    StaticMementos();
    Memento memento[hpack_constants::kLastStaticEntry];
  };
  static const StaticMementos& GetStaticMementos() GPR_ATTRIBUTE_NOINLINE;

  class MementoRingBuffer {
   public:
    // Rebuild this buffer with a new max_entries_ size.
    void Rebuild(uint32_t max_entries);

    // Put a new memento.
    // REQUIRES: num_entries < max_entries
    void Put(Memento m);

    // Pop the oldest memento.
    // REQUIRES: num_entries > 0
    Memento PopOne();

    // Lookup the entry at index, or return nullptr if none exists.
    const Memento* Lookup(uint32_t index) const;

    uint32_t max_entries() const { return max_entries_; }
    uint32_t num_entries() const { return num_entries_; }

   private:
    // The index of the first entry in the buffer. May be greater than
    // max_entries_, in which case a wraparound has occurred.
    uint32_t first_entry_ = 0;
    // How many entries are in the table.
    uint32_t num_entries_ = 0;
    // Maximum number of entries we could possibly fit in the table, given
    // defined overheads.
    uint32_t max_entries_ = hpack_constants::kInitialTableEntries;

    std::vector<Memento> entries_;
  };

  const Memento* LookupDynamic(uint32_t index) const {
    // Not static - find the value in the list of valid entries
    const uint32_t tbl_index = index - (hpack_constants::kLastStaticEntry + 1);
    return entries_.Lookup(tbl_index);
  }

  void EvictOne();

  // The amount of memory used by the table, according to the hpack algorithm
  uint32_t mem_used_ = 0;
  // The max memory allowed to be used by the table, according to the hpack
  // algorithm.
  uint32_t max_bytes_ = hpack_constants::kInitialTableSize;
  // The currently agreed size of the table, according to the hpack algorithm.
  uint32_t current_table_bytes_ = hpack_constants::kInitialTableSize;
  // HPack table entries
  MementoRingBuffer entries_;
  // Mementos for static data
  const StaticMementos& static_metadata_;
};

}  // namespace grpc_core

#endif /* GRPC_CORE_EXT_TRANSPORT_CHTTP2_TRANSPORT_HPACK_PARSER_TABLE_H */
