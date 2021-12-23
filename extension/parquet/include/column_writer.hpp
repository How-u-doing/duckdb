//===----------------------------------------------------------------------===//
//                         DuckDB
//
// column_writer.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
#include "parquet_types.h"

namespace duckdb {
class BufferedSerializer;
class ParquetWriter;
class ColumnWriterPageState;

class ColumnWriterState {
public:
	virtual ~ColumnWriterState();

	vector<uint16_t> definition_levels;
	vector<uint16_t> repetition_levels;
	vector<bool> is_empty;
};

class ColumnWriter {
	//! We limit the uncompressed page size to 100MB
	// The max size in Parquet is 2GB, but we choose a more conservative limit
	static constexpr const idx_t MAX_UNCOMPRESSED_PAGE_SIZE = 100000000;

public:
	ColumnWriter(ParquetWriter &writer, idx_t schema_idx, idx_t max_repeat, idx_t max_define);
	virtual ~ColumnWriter();

	ParquetWriter &writer;
	idx_t schema_idx;
	idx_t max_repeat;
	idx_t max_define;

public:
	//! Create the column writer for a specific type recursively
	static unique_ptr<ColumnWriter> CreateWriterRecursive(vector<duckdb_parquet::format::SchemaElement> &schemas,
	                                                      ParquetWriter &writer, const LogicalType &type,
	                                                      const string &name, idx_t max_repeat = 0,
	                                                      idx_t max_define = 1);

	virtual unique_ptr<ColumnWriterState> InitializeWriteState(duckdb_parquet::format::RowGroup &row_group,
	                                                           vector<string> schema_path);
	virtual void Prepare(ColumnWriterState &state, ColumnWriterState *parent, Vector &vector, idx_t count);

	virtual void BeginWrite(ColumnWriterState &state);
	virtual void Write(ColumnWriterState &state, Vector &vector, idx_t count);
	virtual void FinalizeWrite(ColumnWriterState &state);

protected:
	void HandleDefineLevels(ColumnWriterState &state, ColumnWriterState *parent, ValidityMask &validity, idx_t count,
	                        uint16_t define_value, uint16_t null_value);
	void HandleRepeatLevels(ColumnWriterState &state_p, ColumnWriterState *parent, idx_t count, idx_t max_repeat);

	void WriteLevels(Serializer &temp_writer, const vector<uint16_t> &levels, idx_t max_value, idx_t start_offset,
	                 idx_t count);

	void NextPage(ColumnWriterState &state_p);
	void FlushPage(ColumnWriterState &state_p);

	//! Retrieves the row size of a vector at the specified location. Only used for scalar types.
	virtual idx_t GetRowSize(Vector &vector, idx_t index) = 0;
	//! Writes a (subset of a) vector to the specified serializer. Only used for scalar types.
	virtual void WriteVector(Serializer &temp_writer, ColumnWriterPageState *page_state, Vector &vector,
	                         idx_t chunk_start, idx_t chunk_end) = 0;
	//! Initialize the writer for a specific page. Only used for scalar types.
	virtual unique_ptr<ColumnWriterPageState> InitializePageState();
	//! Flushes the writer for a specific page. Only used for scalar types.
	virtual void FlushPageState(Serializer &temp_writer, ColumnWriterPageState *state);

	void CompressPage(BufferedSerializer &temp_writer, size_t &compressed_size, data_ptr_t &compressed_data,
	                  unique_ptr<data_t[]> &compressed_buf);
};

} // namespace duckdb