#pragma once

#include <Kerberos/KerberosDebug.h>
#include <Kerberos/KClientCompat.h>

#include <stdexcept>

#define BeginShieldedTry_ try { try
#define ShieldedCatch_ catch
#define EndShieldedTry_  } catch (std::exception& e) {                              \
                             dprintf ("Exception '%s' thrown from %s() (%s:%d)",    \
                                      e.what (), __FUNCTION__, __FILE__, __LINE__); \
                             err = paramErr;                                        \
                         } catch (...) {                                            \
                             dprintf ("Exception thrown from %s() (%s:%d)",         \
                                      __FUNCTION__, __FILE__, __LINE__);            \
                             err = kcErrBadParam; }
#define AssertReturnValue_(x) if (!(x)) { SignalCStr_ ("Unhandled error in KClient"); }
	
class KClientError {
	public:
		KClientError (
			OSStatus	inError):
			mError (inError) {}
		OSStatus ErrorCode () const { return mError;}
	
	private:
		OSStatus	mError;
};

class KClientLogicError:
	public std::logic_error,
	public KClientError {
	public:
		KClientLogicError (
			OSStatus	inError):
			std::logic_error ("KClient logic error"),
			KClientError (inError) {}
		int ErrorCode () const { return mError;}
	
	private:
		OSStatus	mError;
};

class KClientRuntimeError:
	public std::runtime_error,
	public KClientError {
	public:
		KClientRuntimeError (
			OSStatus	inError):
			std::runtime_error ("KClient runtime error"),
			KClientError (inError) {}
                        
        protected:
                KClientRuntimeError (
                        OSStatus		inError,
                        const std::string&	inString):
                        std::runtime_error (inString),
                        KClientError (inError) {}
	
};

class KerberosRuntimeError:
	public KClientRuntimeError {
        
	public:
		KerberosRuntimeError (
			OSStatus	inError):
			KClientRuntimeError (inError + kcFirstKerberosError, "KClient runtime error") {}
	
};

class KClientLoginLibRuntimeError:
	public KClientRuntimeError {
	public:
		KClientLoginLibRuntimeError (
			OSStatus	inError):
			KClientRuntimeError (inError, "LoginLib runtime error") {}
};
