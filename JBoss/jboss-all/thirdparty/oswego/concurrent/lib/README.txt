The util.concurrent Release 1.3.2.
http://gee.cs.oswego.edu/dl/classes/EDU/oswego/cs/dl/util/concurrent/intro.html

#  10Jul1998 1.0
# 11Jul1998 1.0.1: removed .class files from release, Fixed documentation error, included Barrier interface.
# 12Jul1998 1.0.2: Fixed return value for swap; fixed documentation errors.
# 15Jul1998 1.0.3: Fixed more documentation errors; re-fixed swap; other cosmetic improvements.
# 18Jul1998 1.0.4: Simplified some classes by removing some alleged optimizations that do not actually help on some platforms; improved SynchronizationTimer; added some documentation.
# 1Sep1998 version 1.1.0:

    * Replace SynchronousChannel algorithm with fairer, more scalable one
    * TimeoutException now extends InterruptedException
    * Replace int counters with longs to avoid wrapping.
    * new LayeredSync class
    * new ObservableSync class
    * new NullSync class
    * new TimeoutSync class
    * new SyncCollection classes
    * new ReentrantWriterPreferenceReadWriteLock class
    * peek added to Channel
    * new ClockDaemon class
    * Refactorings to standardize usage of thread factories
    * removed reliance on ThreadGroups in PooledExecutor 

# 7Jan 1999 Version 1.2

    * ClockDaemon.shutdown allows immediate restart
    * Callable.call throws Throwable, not Exception
    * new Task, TaskRunner, TaskRunnerGroup classes
    * new taskDemo subdirectory 

# 13Jan1999 version 1.2.1

    * Minor cleanup of Task classes 

# 17Jan1999 version 1.2.2:

    * Simplify Task classes; improve documentation; add priority control; they are no longer labeled as `preliminary'.
    * More sample programs in taskDemos
    * Add warnings about reentrancy to RW locks
    * Callable throws Exception again, but FutureResult handles Throwables 

# 25Mar1999 version 1.2.3

    * PooledExecutor -- allow pool to shrink when max size decreased
    * Task -- add reset, array-based operations
    * new PropertyChangeMulticaster, VetoableChangeMulticaster 

# 21may1999 version 1.2.4

    * PooledExecutor -- allow supplied Channel in constructor; new methods createThreads(), drain()
    * Task, TaskRunner, TaskRunnerGroup renamed to FJTask, FJTaskRunner, FJTaskRunnerGroup to avoid clashes with commonly used class name of `Task'.
    * Misc documentation improvements
    * WriterPreferenceReadWriteLock -- fix to notify on interrupt 

# 23oct1999 version 1.2.5

    * PooledExecutor -- add minimumPoolSize settings
    * LU in taskDemo
    * Minor improvements to LinkedQueue, FJTaskRunner 

# 29dec1999 version 1.2.6

    * FJTaskRunner -- now works on MP JVMs that do not correctly implement read-after-write of volatiles.
    * added TimedCallable 

# 12jan2001 version 1.3.0

    * new ConcurrentHashMap, ConcurrentReaderHashMap classes.
    * BoundedLinkedQueue.setCapacity: immediately reconcile permits.
    * ReentrantWriterPreferenceReadWriteLock: Both readers and writers are now reentrant.
    * PooledExecutor: policy now an interface, not abstract class.
    * QueuedExecutor, PooledExecutor: new shutdown methods 

# 2dec2001 Version 1.3.1

    * PooledExecutor: declare inner class constructor as protected, more flexible shutdown support, blocked exec handlers can throw InterruptedExceptions.
    * Ensure all serialization methods are private.
    * Joe Bowbeer's SwingWorker now in misc
    * Improvements to ConcurrentHashMap, ConcurrentReaderHashMap, FIFOReadWriteLock, ReentrantWriterPreferenceReadWriteLock. WaitFreeQueue, SynchronousChannel. 

# 12dec2002 Version 1.3.2

    * SemaphoreControlledChannel - fix constructor to use longs, not its.
    * Improvements to Heap.
    * Fix interference check in ConcurrentReaderHashMap.
    * ReentrantWriterPreferenceReadWriteLock throw IllegalStateException instead of NullPointerException on release errors. 

