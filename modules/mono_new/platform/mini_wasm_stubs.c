#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/*
 * WASM-compatible implementations of mono_file_map and mono_file_unmap.
 *
 * The original implementations in libmonosgen-2.0.a and libmonoutils.a use
 * mmap/munmap, which do not work for Emscripten MEMFS files. These implementations
 * use lseek/read instead, which work correctly on WASM MEMFS.
 *
 * The library symbols were localized (GLOBAL -> LOCAL) via binary patching so
 * that these GLOBAL definitions take precedence for external references.
 */

typedef struct _MonoFileMap MonoFileMap;

/*
 * MonoFileMap is actually just an int fd (file descriptor) cast to a pointer.
 * - mono_file_map_open(path) returns fd as MonoFileMap*
 * - mono_file_map_fd(fmap) returns fmap directly (the fd)
 * - mono_file_map_size(fmap) calls fstat(fd, ...)
 * - mono_file_map_close(fmap) calls close(fd)
 *
 * The library's mono_file_map_open was localized because it may fail on WASM
 * for unknown reasons. This reimplementation adds diagnostics.
 */

/* mono_file_map_fd - return the file descriptor from the MonoFileMap pointer.
 * MonoFileMap is actually int fd cast to pointer.
 * This implementation was added because the library's version may misbehave on WASM.
 */
int mono_file_map_fd(MonoFileMap *fmap) {
	int fd = (int)(long)fmap;
return fd;
}

/*
 * mono_file_map_open - open a file for memory mapping.
 * Returns MonoFileMap* (actually int fd as pointer) on success, NULL on failure.
 */
MonoFileMap *mono_file_map_open(const char *path) {
if (!path) {
		printf("[DIAG] mono_file_map_open: NULL path\n"); fflush(stdout);
return NULL;
	}

	int fd = open(path, O_RDONLY);
if (fd < 0) {
		printf("[DIAG] mono_file_map_open: FAILED to open '%s' (errno=%d)\n", path, errno); fflush(stdout);
return NULL;
	}

	/* Verify file is accessible by reading size */
	struct stat st;
	unsigned long long sz = 0;
	if (fstat(fd, &st) == 0) {
		sz = (unsigned long long)st.st_size;
	}
	/* Also check with lseek */
	off_t old_pos = lseek(fd, 0, SEEK_CUR);
	off_t end_pos = lseek(fd, 0, SEEK_END);
	lseek(fd, old_pos, SEEK_SET);
	printf("[DIAG] mono_file_map_open: opened '%s' fd=%d fstat_size=%llu lseek_size=%lld\n", path, fd, sz, (long long)end_pos); fflush(stdout);

return (MonoFileMap *)(long)fd;
}
/* mono_file_map_size - return the file size.
 * Uses fstat first, then falls back to lseek if fstat returns 0
 * (fstat may return 0 for files in Emscripten MEMFS that were written
 * via Godot's FileAccess but not through standard POSIX write()).
 * Returning 0 causes mono_pe_file_map to fail with "too small: 0", which
 * prevents the PE image from being memory-mapped. The assembly may still
 * load via a fallback path, but method signature resolution from IL tokens
 * fails because the raw metadata is not properly mapped, causing
 * MissingMethodException at runtime.
 */
unsigned long long mono_file_map_size(MonoFileMap *fmap) {
	int fd = (int)(long)fmap;

	/* Try fstat first */
	struct stat st;
	unsigned long long fstat_sz = 0;
	if (fstat(fd, &st) == 0) {
		fstat_sz = (unsigned long long)st.st_size;
	}

	/* Fallback: use lseek to get file size */
	off_t old_pos = lseek(fd, 0, SEEK_CUR);
	off_t end_pos = -1;
	if (old_pos >= 0) {
		end_pos = lseek(fd, 0, SEEK_END);
		lseek(fd, old_pos, SEEK_SET);
	}

	unsigned long long result = 0;
	if (fstat_sz > 0) {
		result = fstat_sz;
	} else if (end_pos > 0) {
		result = (unsigned long long)end_pos;
	}
	printf("[DIAG] mono_file_map_size: fd=%d fstat=%llu lseek=%lld -> returning %llu\n", fd, fstat_sz, (long long)end_pos, result); fflush(stdout);

	return result;
}

/*
 * Correct signature matching libmonosgen-2.0.a:
 *   (i32, i32, i32, i64, i32) -> i32
 *   = (MonoFileMap*, gsize length, gpointer contents, guint64 offset, MonoFileMapError* error) -> gpointer
 *
 * The library's original implementation uses mmap (broken on WASM MEMFS).
 * This implementation uses lseek/read instead.
 */
void *mono_file_map(void *contents, size_t length, MonoFileMap *fmap, unsigned long long offset, void *error) {
	(void)error;

	int fd = mono_file_map_fd(fmap);

	/* mmap semantics: map the ENTIRE file, not just 'length' bytes.
	 * Mono reads PE headers at various offsets (e.g. 0x3C for e_lfanew, 0x80 for PE sig)
	 * which may be beyond 'length'. We must read the full file to avoid out-of-bounds access. */
	size_t file_size = mono_file_map_size(fmap);

	/* Use the larger of file_size and length to ensure we read enough data */
	size_t read_size = (file_size > length) ? file_size : length;
/* Always allocate our own buffer - we must control the memory so mono_file_unmap can free it */
	void *addr = malloc(read_size);
	if (!addr) {
return NULL;
	}

	if (fd < 0) {
free(addr);
		return NULL;
	}

	off_t seek_result = lseek(fd, (off_t)offset, SEEK_SET);
	if (seek_result < 0) {
free(addr);
		return NULL;
	}

	size_t total_read = 0;
	char *buf = (char *)addr;
	while (total_read < read_size) {
		ssize_t bytes_read = read(fd, buf + total_read, read_size - total_read);
		if (bytes_read <= 0) {
			break;
		}
		total_read += (size_t)bytes_read;
	}

	/* If caller provided a buffer, copy data to it as well (for compatibility) */
	if (contents) {
		memcpy(contents, addr, length < total_read ? length : total_read);
	}

	return addr;
}

int mono_file_unmap(void *addr, size_t length) {
(void)length;
	if (addr)
		free(addr);
	return 0;
}

#if defined(__GNUC__) || defined(__clang__)
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

typedef int gint32;
typedef unsigned int guint32;
typedef unsigned long long guint64;
typedef int gboolean;
typedef int mono_bool;
typedef void* gpointer;
typedef size_t gsize;

#define FALSE 0
#define TRUE 1

WEAK void mono_wasm_set_timeout(int timeout, int id) {
	(void)timeout;
	(void)id;
}

WEAK void mono_set_timeout(int timeout, int id) {
	(void)timeout;
	(void)id;
}

WEAK void mono_set_timeout_exec(int id) {
	(void)id;
}

WEAK int getdtablesize(void) { return 1024; }
WEAK int getgrgid(gint32 gid) { (void)gid; return -1; }
WEAK int getgrnam(const char *name) { (void)name; return -1; }
WEAK int getpwnam_r(const char *name, void *pwd, char *buf, gsize buflen, void **result) {
	(void)name; (void)pwd; (void)buf; (void)buflen;
	if (result) *result = NULL;
	return -1;
}
WEAK int getpwuid_r(guint32 uid, void *pwd, char *buf, gsize buflen, void **result) {
	(void)uid; (void)pwd; (void)buf; (void)buflen;
	if (result) *result = NULL;
	return -1;
}
WEAK int inotify_add_watch(int fd, const char *pathname, guint32 mask) {
	(void)fd; (void)pathname; (void)mask;
	return -1;
}
WEAK int inotify_init(void) { return -1; }
WEAK int inotify_rm_watch(int fd, int wd) {
	(void)fd; (void)wd;
	return -1;
}
WEAK int pthread_getschedparam(long thread, int *policy, int *param) {
	(void)thread; (void)policy; (void)param;
	return 0;
}
WEAK int pthread_setschedparam(long thread, int policy, const int *param) {
	(void)thread; (void)policy; (void)param;
	return 0;
}
WEAK int pthread_sigmask(int how, const void *set, void *oset) {
	(void)how; (void)set; (void)oset;
	return 0;
}
WEAK int sem_timedwait(void *sem, const void *abs_timeout) {
	(void)sem; (void)abs_timeout;
	return -1;
}
WEAK int sendfile(int out_fd, int in_fd, long *offset, gsize count) {
	(void)out_fd; (void)in_fd; (void)offset; (void)count;
	return -1;
}
WEAK int sigsuspend(const void *mask) {
	(void)mask;
	return -1;
}

WEAK int mono_arch_cpu_optimizations(int flags) {
	(void)flags;
	return 0;
}

WEAK void mono_arch_cpu_init(void) {
}

WEAK guint32 mono_arch_cpu_enumerate_simd_versions(void) {
	return 0;
}

typedef void* MonoDomain;
typedef void* MonoJitInfo;
typedef void* MonoMethod;
typedef void* MonoContext;
typedef void* MonoLMF;
typedef void* MonoExInfo;
typedef void* MonoFtnDesc;
typedef void* MonoTrampInfo;
typedef void* MonoMethodSignature;
typedef void* MonoMethodRuntimeGenericContext;
typedef void* MonoGSharedvtCallInfo;
typedef void* MonoDelegate;
typedef void* MonoIMTThunk;
typedef void* MonoVTable;

WEAK int mono_arch_use_llvm_backend(void) {
	return 0;
}

WEAK void mono_arch_init(void) {
}

WEAK void mono_arch_finish_init(void) {
}

WEAK void mono_arch_emit_prolog(void *cfg) {
	(void)cfg;
}

WEAK void mono_arch_emit_epilog(void *cfg) {
	(void)cfg;
}

WEAK int mono_arch_need_struct_literal(void *sig) {
	(void)sig;
	return 0;
}

WEAK void mono_arch_patch_imt_method(void *method) {
	(void)method;
}

WEAK void mono_arch_free_jit_info(void *ji) {
	(void)ji;
}

WEAK int mono_arch_is_vtable_trampoline(gpointer addr) {
	(void)addr;
	return 0;
}

WEAK int mono_arch_is_jit_icall_wrapper(gpointer addr) {
	(void)addr;
	return 0;
}

WEAK int mono_arch_is_gsharedvt_wrapper(gpointer addr) {
	(void)addr;
	return 0;
}

WEAK gpointer mono_arch_get_gsharedvt_caller_trampoline(void) {
	return NULL;
}

WEAK int mono_arch_builtin_debugger_support(void) {
	return 0;
}

WEAK void mono_arch_native_to_managed_state(void *sigctx) {
	(void)sigctx;
}

WEAK void mono_arch_managed_to_native_state(void *sigctx) {
	(void)sigctx;
}

WEAK int mono_arch_override_native_signal_handler(void) {
	return 0;
}

WEAK void mono_arch_disable_fp_acc_mode(void) {
}

WEAK int mono_arch_get_breakpoint_trampoline_size(void) {
	return 0;
}

WEAK gpointer mono_arch_context_get_int_reg(MonoContext *ctx, int regnum) {
	(void)ctx; (void)regnum;
	return NULL;
}

WEAK void mono_arch_flush_register_windows(void) {
}

WEAK void mono_arch_register_lowlevel_calls(void) {
}

WEAK void mono_arch_register_icall(void) {
}

WEAK gpointer mono_arch_get_delegate_invoke_impl(MonoMethodSignature *sig, gpointer *addr) {
	(void)sig; (void)addr;
	return NULL;
}

WEAK gpointer mono_arch_get_delegate_virtual_invoke_impl(void *sig, void *method, int offset, gboolean load_imt_reg) {
	(void)sig; (void)method; (void)offset; (void)load_imt_reg;
	return NULL;
}

WEAK gpointer mono_arch_get_gsharedvt_call_info(gpointer addr, void *normal_sig, void *gsharedvt_sig, gboolean gsharedvt_in, gint32 vcall_offset, gboolean calli) {
	(void)addr; (void)normal_sig; (void)gsharedvt_sig; (void)gsharedvt_in; (void)vcall_offset; (void)calli;
	return NULL;
}

WEAK gpointer mono_arch_get_this_arg_from_call(void *regs, void *code) {
	(void)regs; (void)code;
	return NULL;
}

WEAK void mono_arch_patch_code_new(void *cfg, void *domain, void *code, void *ji, void *target) {
	(void)cfg; (void)domain; (void)code; (void)ji; (void)target;
}

WEAK gpointer mono_arch_ip_from_context(void *sigctx) {
	(void)sigctx;
	return NULL;
}

WEAK void mono_arch_find_jit_info(MonoDomain *domain, MonoJitInfo *ji, MonoContext *ctx,
	MonoContext *new_ctx, gpointer *stack_start, gpointer *stack_end,
	MonoLMF **lmf, MonoExInfo *ex) {
	(void)domain; (void)ji; (void)ctx; (void)new_ctx;
	(void)stack_start; (void)stack_end; (void)lmf; (void)ex;
}

WEAK void* mono_arch_find_lmf(void *cur_lmf, void *func) {
	(void)cur_lmf; (void)func;
	return NULL;
}

WEAK int mono_arch_handle_exception(void *sigctx, void *obj) {
	(void)sigctx; (void)obj;
	return 0;
}

WEAK void mono_arch_throw_exception(void *ex, void *original_ctx, void *new_ctx) {
	(void)ex; (void)original_ctx; (void)new_ctx;
}

WEAK gpointer mono_arch_find_imt_method(MonoIMTThunk *imt, gpointer addr) {
	(void)imt; (void)addr;
	return NULL;
}

WEAK gpointer mono_arch_build_imt_trampoline(MonoDomain *domain, MonoMethod *method, MonoIMTThunk *imt_thunk, gpointer *generic_method, gpointer *generic_class) {
	(void)domain; (void)method; (void)imt_thunk; (void)generic_method; (void)generic_class;
	return NULL;
}

WEAK gpointer mono_arch_find_static_call_vtable(void *regs, void *code) {
	(void)regs; (void)code;
	return NULL;
}

WEAK gpointer mono_arch_load_function(int jit_icall_id) {
	(void)jit_icall_id;
	return NULL;
}

WEAK void mono_wasm_debugger_init(void) {
}

WEAK void mono_init_native_crash_info(void) {
}

WEAK void mono_runtime_install_handlers(void) {
}

WEAK void mono_runtime_cleanup_handlers(void) {
}

WEAK void mono_runtime_setup_stat_profiler(void) {
}

WEAK int mono_thread_state_init_from_handle(void *thread, void *jit_info, void *lmf) {
	(void)thread; (void)jit_info; (void)lmf;
	return 0;
}
