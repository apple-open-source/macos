#pragma once

#include <Kerberos/KerberosDebug.h>
#include "KClientCompat.h"

#include <stdexcept>

#define BeginShieldedTry_ try { try
#define ShieldedCatch_ catch
#if MACDEV_DEBUG
#define EndShieldedTry_  } catch (std::exception& e) { SignalPStr_ ("\pUncaught exception."); err = paramErr; dprintf ("%s\n", e.what ()); } \
	catch (...) { SignalPStr_ ("\pUncaught exception."); err = kcErrBadParam; }
#define AssertReturnValue_(x) do {if (!(x)) { SignalPStr_ ("\pUnhandled error in KClient");}} while (false)
#else
#define EndShieldedTry_  } catch (...) { err = paramErr; }
#define AssertReturnValue_(x)
#endif
	
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