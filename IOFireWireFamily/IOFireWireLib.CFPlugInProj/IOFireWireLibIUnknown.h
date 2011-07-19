/*
 *  IOFireWireLibIUnknown.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Thu Feb 27 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 *	$Log: IOFireWireLibIUnknown.h,v $
 *	Revision 1.2  2003/07/21 06:53:10  niels
 *	merge isoch to TOT
 *
 *	Revision 1.1.2.1  2003/07/01 20:54:23  niels
 *	isoch merge
 *	
 */

#import <CoreFoundation/CFPlugInCOM.h>


#define INTERFACEIMP_INTERFACE \
	0,	\
	& IOFireWireIUnknown::SQueryInterface,	\
	& IOFireWireIUnknown::SAddRef,	\
	& IOFireWireIUnknown::SRelease

namespace IOFireWireLib {

	class IOFireWireIUnknown: public IUnknown
	{
		protected:
		
			template<class T>
			class InterfaceMap
			{
				public:
					InterfaceMap( const IUnknownVTbl & vTable, T * inObj ): pseudoVTable(vTable), obj(inObj)		{}
					inline static T * GetThis( void* map )		{ return reinterpret_cast<T*>( reinterpret_cast<InterfaceMap*>( map )->obj ) ; }

				private:
					const IUnknownVTbl &	pseudoVTable ;
					T *						obj ;
			} ;
	
		private:
		
			mutable InterfaceMap<IOFireWireIUnknown>	mInterface ;

		protected:

			UInt32										mRefCount ;

		public:
		
			IOFireWireIUnknown( const IUnknownVTbl & interface ) ;
#if IOFIREWIRELIBDEBUG
			virtual ~IOFireWireIUnknown() ;
#else
			virtual ~IOFireWireIUnknown() {}
#endif			
			virtual HRESULT 							QueryInterface( REFIID iid, LPVOID* ppv ) = 0;
			virtual ULONG 								AddRef() ;
			virtual ULONG 								Release() ;
			
			InterfaceMap<IOFireWireIUnknown>&			GetInterface() const		{ return mInterface ; }
			
			static HRESULT STDMETHODCALLTYPE			SQueryInterface(void* self, REFIID iid, LPVOID* ppv) ;
			static ULONG STDMETHODCALLTYPE				SAddRef(void* self) ;
			static ULONG STDMETHODCALLTYPE				SRelease(void* 	self) ;		
	} ;
	
} // namespace
