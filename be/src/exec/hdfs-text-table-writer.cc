// Copyright (c) 2012 Cloudera, Inc. All rights reserved.

#include "exec/hdfs-text-table-writer.h"
#include "exec/exec-node.h"
#include "util/hdfs-util.h"
#include "exprs/expr.h"
#include "runtime/raw-value.h"
#include "runtime/row-batch.h"
#include "runtime/runtime-state.h"
#include "runtime/hdfs-fs-cache.h"

#include <vector>
#include <sstream>
#include <hdfs.h>
#include <boost/scoped_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <stdlib.h>

#include "gen-cpp/ImpalaService_types.h"

using namespace std;
using namespace boost::posix_time;

namespace impala {
HdfsTextTableWriter::HdfsTextTableWriter(OutputPartition* output,
                                         const HdfsPartitionDescriptor* partition,
                                         const HdfsTableDescriptor* table_desc,
                                         const vector<Expr*>& output_exprs) 
    : HdfsTableWriter::HdfsTableWriter(output, partition, table_desc, output_exprs) {
  tuple_delim_ = partition->line_delim();
  field_delim_ = partition->field_delim();
  escape_char_ = partition->escape_char();
}

Status HdfsTextTableWriter::AppendRow(TupleRow* current_row) {
  stringstream row_stringstream;
  int num_non_partition_cols =
      table_desc_->num_cols() - table_desc_->num_clustering_cols();
  // There might be a select expr for partition cols as well, but we shouldn't be writing
  // their values to the row. Since there must be at least num_non_partition_cols select
  // exprs, and we assume that by convention any partition col exprs are the last in
  // output_exprs_, it's ok to just write the first num_non_partition_cols values.
  for (int j = 0; j < num_non_partition_cols; ++j) {
    void* value = output_exprs_[j]->GetValue(current_row);
    // NULL values become empty strings
    if (value != NULL) {
      output_exprs_[j]->PrintValue(value, &row_stringstream);
    }
    // Append field delimiter.
    if (j + 1 < num_non_partition_cols) {
      row_stringstream << field_delim_;
    }
  }
  // Append tuple delimiter.
  row_stringstream << tuple_delim_;
  // Write line to Hdfs file.
  // HDFS does some buffering to fill a packet which is ~64kb in size. TODO: Determine if
  // there's any throughput benefit in batching larger writes together.
  string row_string = row_stringstream.str();
  RETURN_IF_ERROR(Write(row_string.data(), row_string.size()));
  return Status::OK;
}

}
