// SPDX-License-Identifier: MIT
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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

static void check_wait_set_v3_output(int fd)
{
	struct cpos_wait_set_v3 wait = {
		.size = sizeof(wait), .version = CPOS_ABI_VERSION_WAIT_SET,
		.mode = CPOS_WAIT_ANY, .count = 2,
	};
	__u64 event = 0, sem = 0;
	long page_size = sysconf(_SC_PAGESIZE);
	void *inaccessible;

	CHECK(create_object(fd, CPOS_OBJECT_EVENT, 0, 0, &event) == 0);
	CHECK(create_object(fd, CPOS_OBJECT_SEMAPHORE, 1, 1, &sem) == 0);
	if (!event || !sem || page_size <= 0)
		goto cleanup;
	wait.handles[0] = event;
	wait.handles[1] = sem;
	CHECK(ioctl(fd, CPOS_IOC_WAIT_SET_V3, &wait) == 1);
	CHECK(wait.result_index == 1 && wait.abandoned_mask == 0);
	wait.result_index = 0;
	CHECK(object_op(fd, sem, CPOS_OP_SIGNAL, 0) == 0);
	inaccessible = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(inaccessible != MAP_FAILED);
	if (inaccessible != MAP_FAILED) {
		memcpy(inaccessible, &wait, sizeof(wait));
		CHECK(mprotect(inaccessible, (size_t)page_size, PROT_READ) == 0);
		CHECK(ioctl(fd, CPOS_IOC_WAIT_SET_V3, inaccessible) < 0 && errno == EFAULT);
		CHECK(object_op(fd, sem, CPOS_OP_WAIT, 0) == 0);
		CHECK(munmap(inaccessible, (size_t)page_size) == 0);
	}
cleanup:
	if (sem) drop_object(fd, sem);
	if (event) drop_object(fd, event);
}

static void check_contract_errors(int fd)
{
	struct cpos_object_request request = { .size = sizeof(request) };
	struct cpos_wait_set wait = {
		.size = sizeof(wait), .version = CPOS_ABI_VERSION_EXPERIMENTAL,
		.mode = CPOS_WAIT_ANY, .count = 1,
	};
	__u64 handle = 0;

	request.type = CPOS_OBJECT_SEMAPHORE;
	request.maximum_value = 1;
	request.initial_value = 2;
	CHECK(ioctl(fd, CPOS_IOC_OBJECT_CREATE, &request) < 0 && errno == EINVAL);
	CHECK(create_object(fd, CPOS_OBJECT_EVENT, 0, 0, &handle) == 0);
	if (!handle)
		return;
	wait.handles[0] = handle;
	wait.handles[1] = handle;
	wait.count = 2;
	CHECK(ioctl(fd, CPOS_IOC_WAIT_SET, &wait) < 0 && errno == EINVAL);
	wait.count = 1;
	wait.version = CPOS_ABI_VERSION;
	CHECK(ioctl(fd, CPOS_IOC_WAIT_SET, &wait) < 0 && errno == EINVAL);
	CHECK(object_op(fd, handle, CPOS_OP_RELEASE, 0) < 0 && errno == EINVAL);
	drop_object(fd, handle);
	CHECK(object_op(fd, handle, CPOS_OP_SIGNAL, 0) < 0 && errno == ENOENT);
	CHECK(ioctl(fd, CPOS_IOC_OBJECT_DROP,
		&(struct cpos_object_request){ .size = sizeof(request), .handle = handle }) < 0 && errno == ENOENT);
}

static void check_file_reference(int fd)
{
	static const char payload[] = "CPOS guest file reference";
	char contents[sizeof(payload)] = {0};
	struct cpos_fd_import req = {.size = sizeof(req)};
	struct cpos_object_op op = {.size = sizeof(op), .operation = CPOS_OP_FD_GET};
	int original, received, flags;

	original = memfd_create("cpos-fd-fixture", MFD_CLOEXEC);
	CHECK(original >= 0);
	if (original < 0) return;
	CHECK(write(original, payload, sizeof(payload)) == sizeof(payload));
	CHECK(lseek(original, 0, SEEK_SET) == 0);
	req.fd = original;
	CHECK(ioctl(fd, CPOS_IOC_FD_IMPORT, &req) == 0 && req.handle);
	CHECK(close(original) == 0);
	if (!req.handle) return;
	op.handle = req.handle;
	received = ioctl(fd, CPOS_IOC_OBJECT_OP, &op);
	CHECK(received >= 0);
	drop_object(fd, req.handle);
	if (received < 0) return;
	flags = fcntl(received, F_GETFD);
	CHECK(flags >= 0 && (flags & FD_CLOEXEC));
	CHECK(read(received, contents, sizeof(contents)) == sizeof(contents));
	CHECK(memcmp(contents, payload, sizeof(payload)) == 0);
	CHECK(close(received) == 0);
}

static void check_named_registry(int first_fd)
{
	struct cpos_named_object_request req = {
		.size = sizeof(req), .version = CPOS_ABI_VERSION_NAMED,
		.action = CPOS_NAMED_CREATE_EXCLUSIVE, .type = CPOS_OBJECT_EVENT,
	};
	struct cpos_named_object_request open_req = {
		.size = sizeof(open_req), .version = CPOS_ABI_VERSION_NAMED,
		.action = CPOS_NAMED_OPEN_EXISTING,
	};
	int second_fd, final_fd;
	__u64 first_handle, alias_handle;

	strcpy(req.name, "functional-shared-event");
	CHECK(ioctl(first_fd, CPOS_IOC_NAMED_OBJECT, &req) == 0);
	if (!req.handle)
		return;
	first_handle = req.handle;
	req.handle = 0;
	CHECK(ioctl(first_fd, CPOS_IOC_NAMED_OBJECT, &req) < 0 && errno == EEXIST);
	strcpy(open_req.name, req.name);
	second_fd = open("/dev/compat", O_RDWR | O_CLOEXEC);
	CHECK(second_fd >= 0);
	if (second_fd < 0) {
		drop_object(first_fd, first_handle);
		return;
	}
	CHECK(ioctl(second_fd, CPOS_IOC_NAMED_OBJECT, &open_req) == 0 && open_req.handle);
	if (open_req.handle) {
		struct cpos_wait_set wait = {
			.size = sizeof(wait), .version = CPOS_ABI_VERSION_EXPERIMENTAL,
			.mode = CPOS_WAIT_ANY, .count = 2,
		};

		alias_handle = open_req.handle;
		open_req.handle = 0;
		CHECK(ioctl(second_fd, CPOS_IOC_NAMED_OBJECT, &open_req) == 0 && open_req.handle);
		wait.handles[0] = alias_handle;
		wait.handles[1] = open_req.handle;
		CHECK(ioctl(second_fd, CPOS_IOC_WAIT_SET, &wait) < 0 && errno == EINVAL);
		CHECK(object_op(first_fd, first_handle, CPOS_OP_SIGNAL, 0) == 0);
		CHECK(object_op(second_fd, alias_handle, CPOS_OP_WAIT, 0) == 0);
		drop_object(first_fd, first_handle);
		CHECK(object_op(second_fd, alias_handle, CPOS_OP_RELEASE, 0) == 0);
		CHECK(close(second_fd) == 0);
		final_fd = open("/dev/compat", O_RDWR | O_CLOEXEC);
		CHECK(final_fd >= 0);
		if (final_fd >= 0) {
			open_req.handle = 0;
			CHECK(ioctl(final_fd, CPOS_IOC_NAMED_OBJECT, &open_req) < 0 && errno == ENOENT);
			CHECK(close(final_fd) == 0);
		}
		return;
	}
	drop_object(first_fd, first_handle);
	CHECK(close(second_fd) == 0);
}

static int named_binding(int fd, const char *name, __u32 action, __u32 type,
			__u32 initial, __u32 maximum, __u64 *handle)
{
	struct cpos_named_object_request req = {
		.size = sizeof(req), .version = CPOS_ABI_VERSION_NAMED,
		.action = action, .type = type,
		.initial_value = initial, .maximum_value = maximum,
	};
	strncpy(req.name, name, sizeof(req.name) - 1);
	if (ioctl(fd, CPOS_IOC_NAMED_OBJECT, &req) < 0)
		return -1;
	*handle = req.handle;
	return 0;
}

static void check_named_mutex_abandonment(int first_fd)
{
	struct cpos_wait_set legacy = {
		.size = sizeof(legacy), .version = CPOS_ABI_VERSION_EXPERIMENTAL,
		.mode = CPOS_WAIT_ANY, .count = 1,
	};
	struct cpos_wait_set_v3 wait = {
		.size = sizeof(wait), .version = CPOS_ABI_VERSION_WAIT_SET,
		.mode = CPOS_WAIT_ALL, .count = 2, .timeout_ms = 0,
	};
	__u64 first_mutex = 0, second_mutex = 0, third_mutex = 0;
	__u64 first_sem = 0, second_sem = 0;
	__u64 closing_mutex = 0;
	int close_fd, second_fd = open("/dev/compat", O_RDWR | O_CLOEXEC);

	CHECK(second_fd >= 0);
	if (second_fd < 0)
		return;
	CHECK(named_binding(first_fd, "functional-abandoned-mutex",
		CPOS_NAMED_CREATE_EXCLUSIVE, CPOS_OBJECT_MUTEX, 0, 0, &first_mutex) == 0);
	CHECK(named_binding(second_fd, "functional-abandoned-mutex",
		CPOS_NAMED_OPEN_EXISTING, 0, 0, 0, &second_mutex) == 0);
	if (!first_mutex || !second_mutex)
		goto cleanup;
	legacy.handles[0] = second_mutex;
	CHECK(ioctl(second_fd, CPOS_IOC_WAIT_SET, &legacy) < 0 && errno == EOPNOTSUPP);
	CHECK(object_op(first_fd, first_mutex, CPOS_OP_WAIT, 0) == 0);
	drop_object(first_fd, first_mutex);
	first_mutex = 0;
	CHECK(object_op(second_fd, second_mutex, CPOS_OP_WAIT, 0) < 0 && errno == EOWNERDEAD);
	CHECK(object_op(second_fd, second_mutex, CPOS_OP_RELEASE, 0) == 0);
	close_fd = open("/dev/compat", O_RDWR | O_CLOEXEC);
	CHECK(close_fd >= 0);
	if (close_fd >= 0) {
		CHECK(named_binding(close_fd, "functional-abandoned-mutex",
			CPOS_NAMED_OPEN_EXISTING, 0, 0, 0, &closing_mutex) == 0);
		CHECK(object_op(close_fd, closing_mutex, CPOS_OP_WAIT, 0) == 0);
		CHECK(close(close_fd) == 0);
		CHECK(object_op(first_fd, first_mutex, CPOS_OP_WAIT, 0) < 0 && errno == EOWNERDEAD);
		CHECK(object_op(first_fd, first_mutex, CPOS_OP_RELEASE, 0) == 0);
	}

	CHECK(named_binding(first_fd, "functional-abandoned-mutex",
		CPOS_NAMED_OPEN_EXISTING, 0, 0, 0, &third_mutex) == 0);
	CHECK(named_binding(first_fd, "functional-abandoned-sem",
		CPOS_NAMED_CREATE_EXCLUSIVE, CPOS_OBJECT_SEMAPHORE, 0, 1, &first_sem) == 0);
	CHECK(named_binding(second_fd, "functional-abandoned-sem",
		CPOS_NAMED_OPEN_EXISTING, 0, 0, 0, &second_sem) == 0);
	if (!third_mutex || !first_sem || !second_sem)
		goto cleanup;
	CHECK(object_op(second_fd, second_mutex, CPOS_OP_WAIT, 0) == 0);
	drop_object(second_fd, second_mutex);
	second_mutex = 0;
	wait.handles[0] = third_mutex;
	wait.handles[1] = first_sem;
	CHECK(ioctl(first_fd, CPOS_IOC_WAIT_SET_V3, &wait) < 0 && errno == ETIMEDOUT);
	CHECK(object_op(second_fd, second_sem, CPOS_OP_SIGNAL, 0) == 0);
	wait.abandoned_mask = 0;
	wait.result_index = 0;
	CHECK(ioctl(first_fd, CPOS_IOC_WAIT_SET_V3, &wait) == 0);
	CHECK(wait.abandoned_mask == 1 && wait.result_index == 0);
	CHECK(object_op(second_fd, second_sem, CPOS_OP_SIGNAL, 0) == 0);
	CHECK(object_op(second_fd, second_sem, CPOS_OP_SIGNAL, 0) < 0 && errno == EOVERFLOW);
	CHECK(object_op(first_fd, first_sem, CPOS_OP_WAIT, 0) == 0);
	CHECK(object_op(first_fd, third_mutex, CPOS_OP_RELEASE, 0) == 0);

cleanup:
	if (first_mutex) drop_object(first_fd, first_mutex);
	if (second_mutex) drop_object(second_fd, second_mutex);
	if (third_mutex) drop_object(first_fd, third_mutex);
	if (first_sem) drop_object(first_fd, first_sem);
	if (second_sem) drop_object(second_fd, second_sem);
	CHECK(close(second_fd) == 0);
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
	check_wait_set_v3_output(fd);
	check_contract_errors(fd);
	check_file_reference(fd);
	check_named_registry(fd);
	check_named_mutex_abandonment(fd);
	CHECK(close(fd) == 0);
	if (failures)
		fprintf(stderr, "%d functional assertion(s) failed\n", failures);
	else
		puts("PASS: functional ioctl smoke tests");
	return failures ? 1 : 0;
}
