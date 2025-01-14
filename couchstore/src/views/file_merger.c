/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * @copyright 2013 Couchbase, Inc.
 *
 * @author Filipe Manana  <filipe@couchbase.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 **/

#include "config.h"
#include "util.h"
#include "file_merger.h"

static file_merger_error_t merge_view_files(const char *source_files[],
                                            unsigned num_source_files,
                                            const char *dest_path,
                                            view_file_merge_ctx_t *ctx);


LIBCOUCHSTORE_API
file_merger_error_t merge_view_kvs_ops_files(const char *source_files[],
                                             unsigned num_source_files,
                                             const char *dest_path)
{
    view_file_merge_ctx_t ctx;

    ctx.key_cmp_fun = view_key_cmp;
    ctx.type = INCREMENTAL_UPDATE_VIEW_RECORD;

    return merge_view_files(source_files, num_source_files, dest_path, &ctx);
}


LIBCOUCHSTORE_API
file_merger_error_t merge_view_ids_ops_files(const char *source_files[],
                                             unsigned num_source_files,
                                             const char *dest_path)
{
    view_file_merge_ctx_t ctx;

    ctx.key_cmp_fun = view_id_cmp;
    ctx.type = INCREMENTAL_UPDATE_VIEW_RECORD;

    return merge_view_files(source_files, num_source_files, dest_path, &ctx);
}


static file_merger_error_t merge_view_files(const char *source_files[],
                                            unsigned num_source_files,
                                            const char *dest_path,
                                            view_file_merge_ctx_t *ctx)
{
    return merge_files(source_files, num_source_files, dest_path,
                       read_view_record, write_view_record, NULL,
                       compare_view_records, dedup_view_records_merger,
                       free_view_record, 0, ctx);
}
