// The execection lock is used to control if a thread is executing
// a gdb command (or otherwise mucking about inside gdb). Gdb code is
// not reentrant, so we need to control access to it.

@protocol GuiGdbManagerExecLock <NSObject>

- (void) lockExecLock;
- (void) unlockExecLock;

@end
