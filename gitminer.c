#define PROGRAM_FILE "gitminer.cl"
#define KERNEL_FUNC "sha1_round"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "git.h"

cl_device_id device;
cl_context context;
cl_program program;
cl_kernel kernel;
cl_command_queue queue;
int stopping = 0;

static void destroy()
{
	if (kernel) clReleaseKernel(kernel);
	if (queue) clReleaseCommandQueue(queue);
	if (program) clReleaseProgram(program);
	if (context) clReleaseContext(context);
}

static void halt()
{
	printf("\nReceived signal to stop");
	if (stopping)
	{
		printf(", currently in progress.");
		return;
	}
	printf(", stopping gracefully.");
	stopping = 1;
	exit(1);
}

void format_hash(uint8_t *hash, char *hash_string)
{
	int i;
	for (i = 0; i < 20; i++)
	{
		sprintf(hash_string + i * 2, "%02x", hash[i]);
	}
}
void print_n(uint8_t *hash, int n)
{
	int i;
	for (i = 0; i < n; i++)
	{
		printf("%02x", hash[i]);
	}
	printf("\n");
}

void check_err(cl_int err, const char *message)
{
	if (err < 0)
	{
		printf("\n%d | %s | %s\n", err, get_cl_error_string(err), message);
		halt();
	}
}

cl_device_id create_device()
{
	cl_platform_id platforms[64];
	cl_device_id dev;
	cl_int err;
	cl_uint num_platforms, num_devices;

	err = clGetPlatformIDs(64, platforms, &num_platforms);
	check_err(err, "Couldn't identify a platform");

	err = clGetDeviceIDs(platforms[1], CL_DEVICE_TYPE_GPU, 1, &dev, &num_devices);
	if (err == CL_DEVICE_NOT_FOUND)
	{
		printf("No GPU detected, falling back to CPU\n");
		err = clGetDeviceIDs(platforms[1], CL_DEVICE_TYPE_CPU, 1, &dev, &num_devices);
	}
	check_err(err, "Couldn't access any devices");

	char buffer[1024];
	err = clGetDeviceInfo(dev, CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
	printf("Using device: %s\n", buffer);
	err = clGetDeviceInfo(dev, CL_DEVICE_VERSION, sizeof(buffer), buffer, NULL);
	printf("Version: %s\n\n", buffer);

	return dev;
}

/* Create program from a file and compile it */
cl_program build_program(cl_context ctx, cl_device_id dev, const char *filename)
{
	cl_program program;
	FILE *program_handle;
	char *program_buffer, *program_log;
	size_t program_size, log_size;
	cl_int err;

	/* Read program file and place content into buffer */
	program_handle = fopen(filename, "r");
	if (program_handle == NULL)
	{
		perror("Couldn't find the program file");
		exit(1);
	}
	fseek(program_handle, 0, SEEK_END);
	program_size = ftell(program_handle);
	rewind(program_handle);
	program_buffer = (char *)malloc(program_size + 1);
	program_buffer[program_size] = '\0';
	fread(program_buffer, sizeof(char), program_size, program_handle);
	fclose(program_handle);

	/* Create program from file */
	program = clCreateProgramWithSource(ctx, 1, (const char **)&program_buffer, &program_size, &err);
	check_err(err, "Couldn't create the program");
	free(program_buffer);

	/* Build program */
	err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (err < 0)
	{
		/* Find size of log and print to std output */
		clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
		program_log = (char *) malloc(log_size + 1);
		program_log[log_size] = '\0';
		clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, log_size + 1, program_log, NULL);
		printf("%s\n", program_log);
		free(program_log);
		exit(1);
	}

	return program;
}


double timespec_duration(struct timespec start, struct timespec end)
{
	return (end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1E9;
}

cl_ulong find_nonce(sha1nfo *s, char *hash_str, char *nonce_str)
{
	cl_ulong nonce = 0, offset = 0;

	size_t local_size = 32;
	size_t global_size = 128 * 256;
	size_t hash_bucket_size = 128;
	size_t hash_group_count = global_size * hash_bucket_size;
	int len = s->byteCount, count = 0, i;
	struct timespec start, end, group_start, group_end;
	double time;

	printf("\n--- STARTING | local %lu | global %lu | bucket %lu | group %lu\n", local_size, global_size, hash_bucket_size, hash_group_count);
	clock_gettime(CLOCK_MONOTONIC, &start);

	cl_int err;
	cl_mem nonce_buffer;

	nonce_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(nonce), &nonce, &err);
	check_err(err, "Couldn't create a buffer");

	err = 0;

	while (nonce == 0 && offset < 0xffffffff - hash_group_count)
	{
		if (stopping)
		{
			return 0;
		}
		int arg_count = 0;
		clock_gettime(CLOCK_MONOTONIC, &group_start);

#define CL_SET_ARG(var) err |= clSetKernelArg(kernel, arg_count++, sizeof(cl_uint), (void *)&(var))
		CL_SET_ARG(s->state.w[0]);
		CL_SET_ARG(s->state.w[1]);
		CL_SET_ARG(s->state.w[2]);
		CL_SET_ARG(s->state.w[3]);
		CL_SET_ARG(s->state.w[4]);
		CL_SET_ARG(len);
		err |= clSetKernelArg(kernel, arg_count++, sizeof(cl_ulong), (void *) & (offset));
		CL_SET_ARG(hash_bucket_size);
		err |= clSetKernelArg(kernel, arg_count++, sizeof(cl_mem), &nonce_buffer);
#undef CL_SET_ARG

		check_err(err, "Couldn't create a kernel argument");

		err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
		check_err(err, "Couldn't enqueue the kernel");

		err = clFinish(queue);
		if (err < 0) {
			clReleaseMemObject(nonce_buffer);
			check_err(err, "Couldn't flush");
		}
		err |= clEnqueueReadBuffer(queue, nonce_buffer, CL_TRUE, 0, sizeof(nonce), &nonce, 0, NULL, NULL);
		if (err < 0)
		{
			clReleaseMemObject(nonce_buffer);
			check_err(err, "Couldn't read a buffer");
		}

		count++;
		clock_gettime(CLOCK_MONOTONIC, &group_end);

		time = timespec_duration(group_start, group_end);
		printf("\r%f Mhash/s ", (double)hash_group_count / time / 1E6);
		for (i = 0; i < count / 80 + 1; i++) printf(".");
		printf("%lu", offset);
		offset += hash_group_count;
	}
	if (nonce == 0)
	{
		clReleaseMemObject(nonce_buffer);
		printf("\nSKIPPING | got nonce 0");
		return 0;
	}

	for (i = 0; i < 8; i++)
	{
#define ascii(n, k) ('a' + (((n) >> k * 4) & 0xf))
		nonce_str[i ^ 3] = ascii(nonce, i);
#undef ascii
	}
	nonce_str[8] = 0;

	sha1_write(s, nonce_str, 8);
	sha1_result(s);

	format_hash(s->state.b, hash_str);

	clock_gettime(CLOCK_MONOTONIC, &end);
	time = timespec_duration(start, end);
	printf("\nFOUND %s | %s | %lu hashes | %f seconds | %f Mhash/s\n", hash_str, nonce_str, offset, time, (double) offset / time / 1E6);

	//clReleaseMemObject(hash_buffer);
	clReleaseMemObject(nonce_buffer);
	return nonce;
}

void mine_coins(char *user)
{
	reset();
	add_coin(user);

	char commit[512];
	commit_body(commit);
	//printf("%s\n", commit);

	sha1nfo s;
	commit_hash_outputs(commit, &s);

	char hash_str[128], nonce_str[64];
	if (find_nonce(&s, hash_str, nonce_str) == 0)
	{
		sync_changes(0);
		return;
	}
	//printf("%s // %s\n", nonce_str, hash_str);

	char final_commit[512];
	sprintf(final_commit, "%s%s", commit, nonce_str);
	//printf("%s\n", final_commit);

	perform_commit(final_commit, hash_str);
	sync_changes(1);
}

int main(int argc, char **argv)
{
	if (signal(SIGINT, halt) == SIG_ERR || signal(SIGTERM, halt) == SIG_ERR)
	{
		fputs("An error occurred while setting a signal handler.\n", stderr);
		return EXIT_FAILURE;
	}
	int c;
	char *clone_url, *user;
	while ((c = getopt (argc, argv, "c:u:")) != -1)
	{
		switch (c)
		{
			case 'c':
				clone_url = optarg;
				break;
			case 'u':
				user = optarg;
				break;
			default:
				abort();
		}
	}
	if (clone_url == NULL || user == NULL)
	{
		printf("Usage: gitminer -c <clone_url> -u <username>\n");
		exit(1);
	}
	cl_int err;
	setbuf(stdout, NULL);

	/* Create device and context */
	device = create_device();
	context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
	check_err(err, "Couldn't create a context");
	program = build_program(context, device, PROGRAM_FILE);

	queue = clCreateCommandQueue(context, device, 0, &err);
	check_err(err, "Couldn't create a command queue");

	kernel = clCreateKernel(program, KERNEL_FUNC, &err);
	check_err(err, "Couldn't create a kernel");

	clone(clone_url);
	sync_changes(0);

	while (!stopping)
	{
		mine_coins(user);
	}

	destroy();
	return 0;
}
