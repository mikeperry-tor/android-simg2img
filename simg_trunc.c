/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This tool is meant to be used to strip verity metadata from the end of
 * sparse images. It has not been tested intesively for other use cases */

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE 1
#define _GNU_SOURCE

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sparse/sparse.h>
#include "sparse_file.h"
#include "sparse_defs.h"
#include "backed_block.h"
#include "output_file.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

void usage()
{
  fprintf(stderr, "Usage: simg_trunc <sparse_image_in> <sparse_image_out> <discard_chunks>\n");
}

struct backed_block *backed_block_iter_new(struct backed_block_list *bbl);
struct backed_block *backed_block_iter_next(struct backed_block *bb);
unsigned int backed_block_block(struct backed_block *bb);
int sparse_file_write_block(struct output_file *out, struct backed_block *bb);
unsigned int sparse_count_chunks(struct sparse_file *s);

static int write_n_blocks(struct sparse_file *s, struct output_file *out, int n_blocks,
                          unsigned int trunc_size)
{
  struct backed_block *bb;
  unsigned int last_block = 0;
  int64_t pad;
  int ret = 0;
  int n_written = 0;

  for (bb = backed_block_iter_new(s->backed_block_list); bb;
      bb = backed_block_iter_next(bb)) {

    if (backed_block_block(bb) > last_block) {
      unsigned int blocks = backed_block_block(bb) - last_block;
      write_skip_chunk(out, (int64_t)blocks * s->block_size);
      n_written++;
    }

    last_block = backed_block_block(bb);

    if (n_written >= n_blocks)
      break;

    ret = sparse_file_write_block(out, bb);
    if (ret)
      return ret;
    n_written++;

    last_block = backed_block_block(bb) +
        DIV_ROUND_UP(backed_block_len(bb), s->block_size);

    if (n_written >= n_blocks)
      break;
  }

  pad = trunc_size - (int64_t)last_block * s->block_size;
  assert(pad >= 0);
  if (pad > 0) {
    write_skip_chunk(out, pad);
  }

  return 0;
}

unsigned int sparse_get_trunc_size(struct sparse_file *s, int out_chunks)
{
  struct backed_block *bb;
  unsigned int last_block = 0;
  unsigned int chunks = 0;
  unsigned int trunc_size = 0;

  for (bb = backed_block_iter_new(s->backed_block_list); bb;
      bb = backed_block_iter_next(bb)) {
    if (backed_block_block(bb) > last_block) {
      /* If there is a gap between chunks, add a skip chunk */
      unsigned int blocks = backed_block_block(bb) - last_block;
      trunc_size += blocks*s->block_size;
      chunks++;
    }

    last_block = backed_block_block(bb);
    if (chunks >= out_chunks)
      break;

    chunks++;
    trunc_size += backed_block_len(bb);
    last_block = backed_block_block(bb) +
        DIV_ROUND_UP(backed_block_len(bb), s->block_size);

    if (chunks >= out_chunks)
      break;
  }

  if (last_block < DIV_ROUND_UP(trunc_size, s->block_size)) {
    chunks++;
    trunc_size = s->block_size * DIV_ROUND_UP(trunc_size, s->block_size);
  }

  return trunc_size;
}


int sparse_file_write_trunc(struct sparse_file *s, int fd, bool gz, bool sparse,
    bool crc, int trunc_chunks)
{
  int ret;
  int chunks;
  struct output_file *out;
  unsigned int trunc_size;

  chunks = sparse_count_chunks(s);

  if (trunc_chunks < 0 || chunks <= trunc_chunks)
    return -1;

  chunks -= trunc_chunks;

  trunc_size = sparse_get_trunc_size(s, chunks);

  out = output_file_open_fd(fd, s->block_size, trunc_size, gz, sparse, chunks, crc);

  if (!out)
    return -ENOMEM;

  ret = write_n_blocks(s, out, chunks, trunc_size);

  output_file_close(out);

  printf("Size: %u Blocks: %d Chunks: %d\n", trunc_size,
          trunc_size / s->block_size, chunks);

  return ret;
}


int main(int argc, char *argv[])
{
  int in;
  int out;
  int ret;
  struct sparse_file *s;
  int trunc_chunks;

  if (argc != 4) {
    usage();
    exit(-1);
  }

  trunc_chunks = atoi(argv[3]);

  in = open(argv[1], O_RDONLY | O_BINARY);
  if (in < 0) {
    fprintf(stderr, "Cannot open input file %s\n", argv[1]);
    exit(-1);
  }

  s = sparse_file_import(in, true, false);
  if (!s) {
    fprintf(stderr, "Failed to import sparse file\n");
    exit(-1);
  }

  out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0664);
  if (out < 0) {
      fprintf(stderr, "Cannot open output file %s\n", argv[2]);
      exit(-1);
  }

  ret = sparse_file_write_trunc(s, out, false, true, false, trunc_chunks);

  if (ret) {
      fprintf(stderr, "Failed to write sparse file\n");
      exit(-1);
  }

  close(out);
  close(in);

  exit(0);
}
