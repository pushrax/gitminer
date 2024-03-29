#define SHA1_K0 0x5a827999
#define SHA1_K20 0x6ed9eba1
#define SHA1_K40 0x8f1bbcdc
#define SHA1_K60 0xca62c1d6

#define HASH_LENGTH 20
#define BLOCK_LENGTH 64

#define rol32(n, k) rotate((n), (uint)(k))
#define ascii(n, k) ('a' + (((n) >> k * 4) & 0xf)) << (k * 8 % 32)

__kernel void sha1_round(
	const uint h0,
	const uint h1,
	const uint h2,
	const uint h3,
	const uint h4,
	const uint pre_count,
	const uint offset,
	const uint hash_count,
	__global uint *valid_nonce
)
{
	uint buffer[BLOCK_LENGTH / 4];
	uint state[HASH_LENGTH / 4];
	uint count = pre_count + 8;
	uint nonce = get_global_id(0) * hash_count + offset;
	uint max_nonce = nonce + hash_count;
	//printf("%d - %d - %d\n", get_global_id(0), get_local_id(0), nonce);

	while (nonce < max_nonce)
	{

		buffer[0] = nonce;//ascii(nonce, 0) | ascii(nonce, 1) | ascii(nonce, 2) | ascii(nonce, 3);
		buffer[1] = 0;//ascii(nonce, 4) | ascii(nonce, 5) | ascii(nonce, 6) | ascii(nonce, 7);
		buffer[2] = 0x80000000;
		buffer[3] = 0;
		buffer[4] = 0;
		buffer[5] = 0;
		buffer[6] = 0;
		buffer[7] = 0;
		buffer[8] = 0;
		buffer[9] = 0;
		buffer[10] = 0;
		buffer[11] = 0;
		buffer[12] = 0;
		buffer[13] = 0;
		buffer[14] = 0;
		buffer[15] = count << 3;

		uint a = h0, b = h1, c = h2, d = h3, e = h4, i, t;

#pragma unroll
		for (i = 0; i < 80; i++)
		{
			if (i >= 16)
			{
				t = buffer[(i + 13) & 15] ^ buffer[(i + 8) & 15] ^ buffer[(i + 2) & 15] ^ buffer[i & 15];
				buffer[i & 15] = rol32(t, 1);
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
			t += rol32(a, 5) + e + buffer[i & 15];
			e = d;
			d = c;
			c = rol32(b, 30);
			b = a;
			a = t;
		}

		if (a + h0 == 0 && b + h1 <= 0x05ffffff)
		{
			barrier(CLK_GLOBAL_MEM_FENCE);
			if (valid_nonce[0] != 0) break;
			valid_nonce[0] = nonce;
			break;
		}
		nonce++;
	}

}
