#pragma once

// Override operator new and operator delete on CFM platforms for shared memory
#if TARGET_RT_MAC_CFM
template <class T>
class CCISharedDataAllocator:
	public std::allocator <T> {
	
	public:
	
     CCISharedDataAllocator (
     	const CCISharedDataAllocator&) throw () {}
     	
     CCISharedDataAllocator () throw () {}

     ~CCISharedDataAllocator () throw () {}
};
#endif // TARGET_RT_MAC_CFM

class CCISharedData {
// Override operator new and operator delete on CFM platforms for shared memory
#if TARGET_RT_MAC_CFM
	public:
		// Override operator new to force allocation in system heap
		void* operator new (std::size_t inSize) throw (std::bad_alloc) {

			for(;;) {
				try {
					return sAllocator.allocate (inSize, 0);
				} catch (std::bad_alloc&) {
					std::new_handler handler = std::set_new_handler (NULL);
					std::set_new_handler (handler);
					
					if(handler == NULL) {
						throw;
					}
					handler ();
				}
			}
			CCIDEBUG_SIGNAL ("Fell out the bottom of CCISharedData::operator new");
			throw std::bad_alloc ();
		}

		// Override placement new because we overrode new
		void* operator new (std::size_t, void* inPointer) throw () {
			return inPointer;
		}

		// Override operator delete to deallocate from system heap
		void  operator delete (void* inPointer) throw () {
			
			sAllocator.deallocate (reinterpret_cast <char*> (inPointer), 0);
		}
    
private:
    static CCISharedDataAllocator <char>		sAllocator;
#endif // TARGET_RT_MAC_CFM
};

class CCICCacheData;

// Some of this doesn't compile on Mac OS X DP4 because of STL problems,.
// But we don't need it anyway
#if !TARGET_RT_MAC_MACHO
namespace CallImplementations {
	class String {
		public:
		typedef std::basic_string <char, std::char_traits <char>,
			CCISharedDataAllocator <char> >												Shared;
	};
	template <class T>
	class List {
		public:
		typedef	typename std::list <T, CCISharedDataAllocator <T> >						Shared;
	};
	template <class T, class U>
	class Map {
		public:
		typedef	typename std::map <T, U, 
			std::less <T>, CCISharedDataAllocator <std::pair <const T, U> > >			Shared;
	};
	template <class T>
	class Vector {
		public:
		typedef typename std::vector <T, CCISharedDataAllocator <T> >					Shared;
	};
	template <class T>
	class Deque {
		public:
		typedef typename std::deque <T, CCISharedDataAllocator <T> >					Shared;
	};
	template <class T>
	class Allocator {
		public:
		typedef typename CCISharedDataAllocator <T>										Shared;
	};

	typedef CCISharedData																SharedData;
}
#endif // !TARGET_RT_MAC_MACHO

namespace MachIPCImplementations {
	class String {
		public:
                typedef std::string					Shared;
	};
	template <class T>
	class List {
		public:
		typedef	typename std::list <T, std::allocator <T> >						Shared;
	};
	template <class T, class U>
	class Map {
		public:
		typedef	typename std::map <T, U, 
			std::less <T>, std::allocator <std::pair <const T, U> > >			Shared;
	};
	template <class T>
	class Vector {
		public:
		typedef typename std::vector <T, std::allocator <T> >					Shared;
	};
	template <class T>
	class Deque {
		public:
		typedef typename std::deque <T, std::allocator <T> >					Shared;
	};
	template <class T>
	class Allocator {
		public:
                typedef std::allocator <T> Shared;
	};

	typedef CCISharedData																SharedData;
}
