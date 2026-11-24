// SPDX-License-Identifier: MIT
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../cpos_abi.h"

static int failures;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "FAIL %s:%d: %s (errno=%d: %s)\n", \
			__FILE__, __LINE__, #expr, errno, strerror(errno)); \
		failures++; \
	} \
} while (0)

static int create_object(int fd, __u32 type, __u32 initial, __u32 maximum,
			 __u64 *handle)
{
	struct cpos_object_request req = {
		.size = sizeof(req), .type = type,
		.initial_value = initial, .maximum_value = maximum,
	};
	if (ioctl(fd, CPOS_IOC_OBJECT_CREATE, &req) < 0)
		return -1;
	*handle = req.handle;
	return 0;
}

static int object_op(int fd, __u64 handle, __u32 operation, __u32 timeout)
{
	struct cpos_object_op op = {
		.size = sizeof(op), .operation = operation,
		.handle = handle, .timeout_ms = timeout,
	};
	return ioctl(fd, CPOS_IOC_OBJECT_OP, &op);
}

static void drop_object(int fd, __u64 handle)
{
	struct cpos_object_request req = { .size = sizeof(req), .handle = handle };
	CHECK(ioctl(fd, CPOS_IOC_OBJECT_DROP, &req) == 0);
}

static void check_wait_sets(int fd)
{
	__u64 event = 0, sem = 0;
	struct cpos_wait_set req = {
		.size = sizeof(req), .version = CPOS_ABI_VERSION_EXPERIMENTAL,
		.mode = CPOS_WAIT_ALL, .count = 2,
	};

	CHECK(create_object(fd, CPOS_OBJECT_EVENT, 0, 0, &event) == 0);
	CHECK(create_object(fd, CPOS_OBJECT_SEMAPHORE, 1, 1, &sem) == 0);
	if (event && sem) {
		req.handles[0] = event;
		req.handles[1] = sem;
		CHECK(ioctl(fd, CPOS_IOC_WAIT_SET, &req) < 0 && errno == ETIMEDOUT);
		req.mode = CPOS_WAIT_ANY;
		CHECK(ioctl(fd, CPOS_IOC_WAIT_SET, &req) == 1);
		CHECK(ioctl(fd, CPOS_IOC_WAIT_SET, &req) < 0 && errno == ETIMEDOUT);
		CHECK(object_op(fd, event, CPOS_OP_SIGNAL, 0) == 0);
		CHECK(object_op(fd, sem, CPOS_OP_SIGNAL, 0) == 0);
		req.mode = CPOS_WAIT_ALL;
		CHECK(ioctl(fd, CPOS_IOC_WAIT_SET, &req) == 0);
		CHECK(object_op(fd, sem, CPOS_OP_WAIT, 0) < 0 && errno == ETIMEDOUT);
		CHECK(object_op(fd, event, CPOS_OP_WAIT, 0) == 0);
	}
	if (sem) drop_object(fd, sem);
	if (event) drop_object(fd, event);
}

int main(void)
{
	struct cpos_abi_info info = { .size = sizeof(info) };
	struct cpos_token_request token = { .size = sizeof(token) };
	__u64 handle;
	int fd = open("/dev/compat", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("open /dev/compat");
		return 1;
	}
	CHECK(ioctl(fd, CPOS_IOC_QUERY, &info) == 0);
	CHECK(info.version == CPOS_ABI_VERSION && info.max_tokens == CPOS_MAX_TOKENS);
	CHECK(ioctl(fd, CPOS_IOC_CREATE, &token) == 0 && token.token != 0);
	if (token.token)
		CHECK(ioctl(fd, CPOS_IOC_DROP, &token) == 0);

	if (create_object(fd, CPOS_OBJECT_EVENT, 0, 0, &handle) == 0) {
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) < 0 && errno == ETIMEDOUT);
		CHECK(object_op(fd, handle, CPOS_OP_SIGNAL, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_RELEASE, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) < 0 && errno == ETIMEDOUT);
		drop_object(fd, handle);
	} else CHECK(0 && "create event");
	if (create_object(fd, CPOS_OBJECT_SEMAPHORE, 1, 1, &handle) == 0) {
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) < 0 && errno == ETIMEDOUT);
		CHECK(object_op(fd, handle, CPOS_OP_SIGNAL, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_SIGNAL, 0) < 0 && errno == EOVERFLOW);
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) == 0);
		drop_object(fd, handle);
	} else CHECK(0 && "create semaphore");
	if (create_object(fd, CPOS_OBJECT_MUTEX, 0, 0, &handle) == 0) {
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) < 0 && errno == ETIMEDOUT);
		CHECK(object_op(fd, handle, CPOS_OP_RELEASE, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_WAIT, 0) == 0);
		CHECK(object_op(fd, handle, CPOS_OP_RELEASE, 0) == 0);
		drop_object(fd, handle);
	} else CHECK(0 && "create mutex");
	check_wait_sets(fd);
	CHECK(close(fd) == 0);
	if (failures)
		fprintf(stderr, "%d functional assertion(s) failed\n", failures);
	else
		puts("PASS: functional ioctl smoke tests");
	return failures ? 1 : 0;
}
