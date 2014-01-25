/* This code is public-domain - it is based on libcrypt
 * placed in the public domain by Wei Dai and other contributors.
 */
// gcc -Wall -DSHA1TEST -o sha1test sha1.c && ./sha1test

#include <stdint.h>
#include <string.h>

#include "git.h"

/* code */
#define SHA1_K0 0x5a827999
#define SHA1_K20 0x6ed9eba1
#define SHA1_K40 0x8f1bbcdc
#define SHA1_K60 0xca62c1d6

const uint8_t sha1InitState[] =
{
	0x01, 0x23, 0x45, 0x67, // H0
	0x89, 0xab, 0xcd, 0xef, // H1
	0xfe, 0xdc, 0xba, 0x98, // H2
	0x76, 0x54, 0x32, 0x10, // H3
	0xf0, 0xe1, 0xd2, 0xc3 // H4
};

void sha1_init(sha1nfo *s)
{
	memcpy(s->state.b, sha1InitState, HASH_LENGTH);
	s->byteCount = 0;
	s->bufferOffset = 0;
}

uint32_t sha1_rol32(uint32_t number, uint8_t bits)
{
	return ((number << bits) | (number >> (32 - bits)));
}

void sha1_hashBlock(sha1nfo *s)
{
	uint8_t i;
	uint32_t a, b, c, d, e, t;

	a = s->state.w[0];
	b = s->state.w[1];
	c = s->state.w[2];
	d = s->state.w[3];
	e = s->state.w[4];
	for (i = 0; i < 80; i++)
	{
		if (i >= 16)
		{
			t = s->buffer.w[(i + 13) & 15] ^ s->buffer.w[(i + 8) & 15] ^ s->buffer.w[(i + 2) & 15] ^ s->buffer.w[i & 15];
			s->buffer.w[i & 15] = sha1_rol32(t, 1);
		}
		if (i < 20)
		{
			t = (d ^ (b & (c ^ d))) + SHA1_K0;
		}
		else if (i < 40)
		{
			t = (b ^ c ^ d) + SHA1_K20;
		}
		else if (i < 60)
		{
			t = ((b & c) | (d & (b | c))) + SHA1_K40;
		}
		else
		{
			t = (b ^ c ^ d) + SHA1_K60;
		}
		t += sha1_rol32(a, 5) + e + s->buffer.w[i & 15];
		e = d;
		d = c;
		c = sha1_rol32(b, 30);
		b = a;
		a = t;
	}
	s->state.w[0] += a;
	s->state.w[1] += b;
	s->state.w[2] += c;
	s->state.w[3] += d;
	s->state.w[4] += e;
}

void sha1_addUncounted(sha1nfo *s, uint8_t data)
{
	s->buffer.b[s->bufferOffset ^ 3] = data;
	s->bufferOffset++;
	if (s->bufferOffset == BLOCK_LENGTH)
	{
		sha1_hashBlock(s);
		s->bufferOffset = 0;
	}
}

void sha1_writebyte(sha1nfo *s, uint8_t data)
{
	++s->byteCount;
	sha1_addUncounted(s, data);
}

void sha1_write(sha1nfo *s, const char *data, size_t len)
{
	for (; len--;)
	{
		sha1_writebyte(s, (uint8_t) *data++);
	}
}

void sha1_pad(sha1nfo *s)
{
	// Implement SHA-1 padding (fips180-2 §5.1.1)

	// Pad with 0x80 followed by 0x00 until the end of the block
	sha1_addUncounted(s, 0x80);
	while (s->bufferOffset != 56) sha1_addUncounted(s, 0x00);

	// Append length in the last 8 bytes
	sha1_addUncounted(s, 0); // We're only using 32 bit lengths
	sha1_addUncounted(s, 0); // But SHA-1 supports 64 bit lengths
	sha1_addUncounted(s, 0); // So zero pad the top bits
	sha1_addUncounted(s, s->byteCount >> 29); // Shifting to multiply by 8
	sha1_addUncounted(s, s->byteCount >> 21); // as SHA-1 supports bitstreams as well as
	sha1_addUncounted(s, s->byteCount >> 13); // byte.
	sha1_addUncounted(s, s->byteCount >> 5);
	sha1_addUncounted(s, s->byteCount << 3);
}

uint8_t *sha1_result(sha1nfo *s)
{
	int i;
	// Pad to complete the last block
	sha1_pad(s);

	// Swap byte order back
	for (i = 0; i < 5; i++)
	{
		uint32_t a, b;
		a = s->state.w[i];
		b = a << 24;
		b |= (a << 8) & 0x00ff0000;
		b |= (a >> 8) & 0x0000ff00;
		b |= a >> 24;
		s->state.w[i] = b;
	}

	// Return pointer to hash (20 characters)
	return s->state.b;
}

/* self-test */

#if SHA1TEST
#include <stdio.h>

void printHash(uint8_t *hash)
{
	int i;
	for (i = 0; i < 20; i++)
	{
		printf("%02x", hash[i]);
	}
	printf("\n");
}


int main (int argc, char **argv)
{
	uint32_t a;
	sha1nfo s;

	// SHA tests
	printf("Test: FIPS 180-2 C.1 and RFC3174 7.3 TEST1\n");
	printf("Expect:a9993e364706816aba3e25717850c26c9cd0d89d\n");
	printf("Result:");
	sha1_init(&s);
	sha1_write(&s, "abc", 3);
	printHash(sha1_result(&s));
	printf("\n\n");
	return 0;

	printf("Test: FIPS 180-2 C.2 and RFC3174 7.3 TEST2\n");
	printf("Expect:84983e441c3bd26ebaae4aa1f95129e5e54670f1\n");
	printf("Result:");
	sha1_init(&s);
	sha1_write(&s, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
	printHash(sha1_result(&s));
	printf("\n\n");

	printf("Test: RFC3174 7.3 TEST4\n");
	printf("Expect:dea356a2cddd90c7a7ecedc5ebb563934f460452\n");
	printf("Result:");
	sha1_init(&s);
	for (a = 0; a < 80; a++) sha1_write(&s, "01234567", 8);
	printHash(sha1_result(&s));
	printf("\n\n");

	// Long tests
	printf("Test: FIPS 180-2 C.3 and RFC3174 7.3 TEST3\n");
	printf("Expect:34aa973cd4c4daa4f61eeb2bdbad27316534016f\n");
	printf("Result:");
	sha1_init(&s);
	for (a = 0; a < 1000000; a++) sha1_writebyte(&s, 'a');
	printHash(sha1_result(&s));

	return 0;
}
#endif /* self-test */
