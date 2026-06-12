#if defined(__arm__)
int process_vm_readv_syscall = 376;
int process_vm_writev_syscall = 377;
#elif defined(__aarch64__)
int process_vm_readv_syscall = 270;
int process_vm_writev_syscall = 271;
#elif defined(__i386__)
int process_vm_readv_syscall = 347;
int process_vm_writev_syscall = 348;
#else
int process_vm_readv_syscall = 310;
int process_vm_writev_syscall = 311;
#endif

ssize_t process_v(pid_t __pid, const struct iovec *__local_iov, unsigned long __local_iov_count,
				  const struct iovec *__remote_iov, unsigned long __remote_iov_count,
				  unsigned long __flags, bool iswrite)
{
	return syscall((iswrite ? process_vm_writev_syscall : process_vm_readv_syscall), __pid,
				   __local_iov, __local_iov_count, __remote_iov, __remote_iov_count, __flags);
}
bool pvm(void *address, void *buffer, size_t size, bool iswrite)
{
	struct iovec local[1];
	struct iovec remote[1];
	local[0].iov_base = buffer;
	local[0].iov_len = size;
	remote[0].iov_base = address;
	remote[0].iov_len = size;
	if (getpid() < 0)
	{
		return false;
	}
	ssize_t bytes = process_v(getpid(), local, 1, remote, 1, 0, iswrite);
	return bytes == size;
}

bool vm_readv(unsigned long address, void *buffer, size_t size)
{
	return pvm(reinterpret_cast < void *>(address), buffer, size, false);
}
bool vm_writev(unsigned long address, void *buffer, size_t size)
{
	return pvm(reinterpret_cast < void *>(address), buffer, size, true);
}
long int getZZ(long long addr) {
	long long var[1] = {0};
	vm_readv(addr, var, 8);
	return var[0];
}

float getFloat(long int addr) {
	float var[1] = {0};
	vm_readv(addr, var, 4);
	return var[0];
}

int getDword(long int addr) {
	long int var[1] = { 0 };
	vm_readv(addr, var, 4);
	return var[0];
}
void WriteFloat(long addr, float data)
{
	vm_writev(addr, &data, 4);
}

// 写入D类内存
void WriteDword(long addr, int data)
{
	vm_writev(addr, &data, 4);
}
