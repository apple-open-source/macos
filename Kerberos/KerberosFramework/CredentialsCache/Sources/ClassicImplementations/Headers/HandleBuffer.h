#pragma once

#if TARGET_RT_MAC_MACHO
//#include <mach/message.h>
#endif

/* 
 * A handle-based buffer used for message sending and receiving in
 * Classic ticket sharing
 */

class CCIHandleBuffer {
	public:
		CCIHandleBuffer ();

		~CCIHandleBuffer ();

		void Reset ();

		// Put various types in the buffer
#if TARGET_RT_MAC_MACHO
// Currently: mach_msg_type_number_t == CCITime
//		void Put (
//			mach_msg_type_number_t	inData);
#endif
		void Put (
			const CCIUniqueID&		inData);
		void Put (
			CCITime					inData);
		void Put (
			CCIResult			inData);
		void Put (
			const std::string&		inData);
		void Put (
			bool				inData);
		void Put (
			const std::vector <CCIObjectID>&	inData);
		void Put (
			std::strstream&			outDate);

		// Get various types from the buffer		
		void Get (
			CCIUniqueID&			outData) const;
		void Get (
			CCITime&			outData) const;
		void Get (
			CCIResult&			outData) const;
		void Get (
			std::string&			outData) const;
		void Get (
			bool&				outData) const;
		void Get (
			std::vector <CCIObjectID>&	outData) const;
		void Get (
			std::strstream&			outDate) const;
		
		// Low level buffer access
		void GetData (
			void*		ioBuffer,
			CCIUInt32	inSize) const;

		void PutData (
			const void*	ioBuffer,
			CCIUInt32	inSize);

		// Access to the buffer handle
		Handle GetHandle () const;
		void DisposeHandle ();
		void ReleaseHandle ();
		void AdoptHandle (
			Handle		inNewHandle);
			
		void UpdateSize ();
		void SetOffset (
			CCIUInt32	inOffset);
		
	private:
	
				Handle		mHandle;
				CCIUInt32	mSize;
		// offset is changed by buffer readers which are const
		mutable	CCIUInt32	mOffset;
		
		CCIHandleBuffer (const CCIHandleBuffer&);
		const CCIHandleBuffer& operator = (const CCIHandleBuffer&);
};
