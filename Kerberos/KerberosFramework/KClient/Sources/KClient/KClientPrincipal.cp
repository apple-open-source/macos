#include "KClientPrincipal.h"
#include "KClientPrincipalPriv.h"

KClientPrincipalPriv::KClientPrincipalPriv ()
{
	mMagic = class_ID;
	mValid = false;
}

KClientPrincipalPriv::KClientPrincipalPriv (
	const char*						inName,
	const char*						inInstance,
	const char*						inRealm)
{
	mMagic = class_ID;
	strcpy (mName, inName);
	strcpy (mInstance, inInstance);
	strcpy (mRealm, inRealm);
	mValid = true;
}

KClientPrincipalPriv::KClientPrincipalPriv (
	const char*						inPrincipal)
{
	mMagic = class_ID;
	int err = kname_parse (mName, mInstance, mRealm, const_cast <char*> (inPrincipal));
	if (err != KSUCCESS)
		throw -1;
	mValid = true;
}

KClientPrincipalPriv::KClientPrincipalPriv (
	const KClientPrincipalPriv&		inOriginal)
{
	inOriginal.CheckValid ();
		
	strcpy (mName, inOriginal.mName);
	strcpy (mInstance, inOriginal.mInstance);
	strcpy (mRealm, inOriginal.mRealm);
	mValid = true;
}

Boolean
KClientPrincipalPriv::IsValid () const
{
	return mValid;
}

void
KClientPrincipalPriv::GetTriplet (
	char*		outName,
	char*		outInstance,
	char*		outRealm)
{
	CheckValid ();
	
	strcpy (outName, mName);
	strcpy (outInstance, mInstance);
	strcpy (outRealm, mRealm);
}

void
KClientPrincipalPriv::GetString (
	EPrincipalComponents	inComponents,		
	char*					outPrincipal)
{
	CheckValid ();
	
	if ((inComponents & ~(principal_Instance | principal_Realm)) != 0)
		throw -1;
		
	int err = kname_unparse (outPrincipal, mName,
		((inComponents & principal_Instance) != 0) ? mInstance : nil,
		((inComponents & principal_Realm) != 0) ? mRealm : nil);
	Assert_ (err == KSUCCESS);
}

void
KClientPrincipalPriv::GetKLPrincipalInfo (
	KLPrincipalInfo*				outPrincipalInfo)
{
	CheckValid ();
	
	strcpy ((char*) outPrincipalInfo -> principal, mName);
	strcpy ((char*) outPrincipalInfo -> instance, mInstance);
	strcpy ((char*) outPrincipalInfo -> realm, mRealm);
	
	c2pstr ((char*) outPrincipalInfo -> principal);
	c2pstr ((char*) outPrincipalInfo -> instance);
	c2pstr ((char*) outPrincipalInfo -> realm);
}
	
Boolean
KClientPrincipalPriv::IsInitialTGT () const
{
	CheckValid ();

	return ((strcmp (mName, KRB_TICKET_GRANTING_TICKET) == 0) &&
	        (strcmp (mInstance, mRealm) == 0));
}

const char*
KClientPrincipalPriv::GetName () const
{
	CheckValid ();
	
	return mName;
}

const char*
KClientPrincipalPriv::GetInstance () const
{
	CheckValid ();
	
	return mInstance;
}

const char*
KClientPrincipalPriv::GetRealm () const
{
	CheckValid ();
	
	return mRealm;
}

void KClientPrincipalPriv::CheckValid () const
{
	if (!IsValid ())
		throw -1;
}
		

StKClientPrincipal::StKClientPrincipal (
	KClientPrincipal				inPrincipal)
{
	mPrincipal = reinterpret_cast <KClientPrincipalPriv*> (inPrincipal);
	if (!ValidatePrincipal ())
		throw -1;
}

KClientPrincipalPriv*
StKClientPrincipal::operator -> ()
{
	return mPrincipal;
}

const KClientPrincipalPriv*
StKClientPrincipal::operator -> () const
{
	return mPrincipal;
}

StKClientPrincipal::operator KClientPrincipal ()
{
	return reinterpret_cast <KClientPrincipal> (mPrincipal);
}

StKClientPrincipal::operator KClientPrincipalPriv& ()
{
	return *mPrincipal;
}

Boolean
StKClientPrincipal::ValidatePrincipal () const
{
	return mPrincipal -> mMagic == KClientPrincipalPriv::class_ID;
}

Boolean
operator == (
	const KClientPrincipalPriv&		inLeft,
	const KClientPrincipalPriv&		inRight)
{
	inLeft.CheckValid ();
	inRight.CheckValid ();
		
	return ((strcmp (inLeft.mName, inRight.mName) == 0) &&
	        (strcmp (inLeft.mInstance, inRight.mInstance) == 0) &&
	        (strcmp (inLeft.mRealm, inRight.mRealm) == 0));
}

