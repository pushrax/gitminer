#ifndef GIT_H
#define GIT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#ifdef MAC
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

void print_n(uint8_t *hash, int n);

void run(char *command);
void gclone(char *origin);
void reset();
void add_coin(char *user);


#define HASH_LENGTH 20
#define BLOCK_LENGTH 64

union _buffer
{
	uint8_t b[BLOCK_LENGTH];
	uint32_t w[BLOCK_LENGTH / 4];
};

union _state
{
	uint8_t b[HASH_LENGTH];
	uint32_t w[HASH_LENGTH / 4];
};

typedef struct sha1nfo
{
	union _buffer buffer;
	uint8_t bufferOffset;
	union _state state;
	uint32_t byteCount;
	uint8_t keyBuffer[BLOCK_LENGTH];
	uint8_t innerHash[HASH_LENGTH];
} sha1nfo;

void sha1_init(sha1nfo *s);
void sha1_writebyte(sha1nfo *s, uint8_t data);
void sha1_write(sha1nfo *s, const char *data, size_t len);
uint8_t *sha1_result(sha1nfo *s);

void commit_body(char *buffer, char *node_id);
void commit_hash_outputs(char *commit, sha1nfo *s);
void perform_commit(char *commit, char *sha, int len);
void sync_changes(int push);

const char *get_cl_error_string(cl_int err);

#endif
