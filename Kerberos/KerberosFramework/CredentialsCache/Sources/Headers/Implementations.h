/*
 * CCIImplementaions.h
 *
 * This header file encapsulates the knowledge of what implementations
 * of the CCache library are available, and which one is used.
 *
 * This is done using namespaces. For each implementation, we declare
 * an empty namespace; classes comprising the implementations
 * add themselves to the appropriate namespace at the place where they are declared.
 *
 * Then, we pick one of the namespaces and add an alias to it called
 * Implementations. By changing which implementation is aliased to Implementations,
 * we change which implementation is used throughout.
 */

namespace	CallImplementations {
}

namespace	AEImplementations {
}

namespace	MachIPCImplementations {
}

#if defined(CCacheUsesMachIPC) && CCacheUsesMachIPC
namespace	Implementations = MachIPCImplementations;
#elif defined (CCacheUsesAppleEvents) && CCacheUsesAppleEvents
namespace	Implementations = AEImplementations;
#else
namespace	Implementations = CallImplementations;
#endif

