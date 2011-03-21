/*
 *  CACNGApplet.h
 *  Tokend
 *
 *  Created by harningt on 9/30/09.
 *  Copyright 2009 TrustBearer Labs. All rights reserved.
 *
 */
#ifndef CACNGAPPLET_H
#define CACNGAPPLET_H

#include "byte_string.h"
#include <security_utilities/utilities.h>

#include <tr1/memory>
using std::tr1::shared_ptr;

class CACNGToken;

class CACNGSelectable
{
	NOCOPY(CACNGSelectable)
public:
	CACNGSelectable() {}
	virtual ~CACNGSelectable() {}

protected:
	virtual void select() = 0;
	friend class CACNGToken;
};

class CACNGReadable
{
	NOCOPY(CACNGReadable)
public:
	CACNGReadable() {}
	virtual ~CACNGReadable() {}
	virtual byte_string read() = 0;
};

class CACNGCryptable
{
	NOCOPY(CACNGCryptable)
public:
	CACNGCryptable() {}
	virtual ~CACNGCryptable() {}
	virtual byte_string crypt(const byte_string &input) = 0;
};

class CACNGCacApplet : public CACNGSelectable
{
	NOCOPY(CACNGCacApplet);
public:
	CACNGCacApplet(CACNGToken &token, const byte_string &applet, const byte_string &object);
	virtual ~CACNGCacApplet() {}

protected:
	void select();

	CACNGToken &token;
private:
	const byte_string applet;
	const byte_string object;
};


class CACNGPivApplet : public CACNGSelectable
{
	NOCOPY(CACNGPivApplet)
public:
	CACNGPivApplet(CACNGToken &token, const byte_string &applet);
	virtual ~CACNGPivApplet() {}
	
protected:
	CACNGToken &token;
	void select();

private:
	const byte_string applet;
};

class CACNGIDObject : public CACNGReadable, public CACNGCryptable
{
	NOCOPY(CACNGIDObject);
public:
	CACNGIDObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, const std::string &description);

	size_t getKeySize();
protected:
	CACNGToken &token;
	shared_ptr<CACNGSelectable> applet;
	
	size_t keySize;
	const std::string description;	
};

class CACNGCacIDObject : public CACNGIDObject
{
	NOCOPY(CACNGCacIDObject);
public:
	CACNGCacIDObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, const std::string &description);
	virtual ~CACNGCacIDObject() {}
	byte_string read();
	byte_string crypt(const byte_string &input);
};

class CACNGPivIDObject : public CACNGIDObject
{
	NOCOPY(CACNGPivIDObject)
public:
	CACNGPivIDObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, const std::string &description, const byte_string &oid, uint8_t keyRef);
	virtual ~CACNGPivIDObject() {}

	byte_string read();
	byte_string crypt(const byte_string &input);
private:
	const byte_string oid;
	const uint8_t keyRef;
};

class CACNGCacBufferObject : public CACNGReadable
{
	NOCOPY(CACNGCacBufferObject);
public:
	CACNGCacBufferObject(CACNGToken &token, shared_ptr<CACNGSelectable> applet, bool isTbuffer);
	virtual ~CACNGCacBufferObject() {}

	byte_string read();
private:
	CACNGToken &token;
	shared_ptr<CACNGSelectable> applet;
	bool isTbuffer;
};

#endif /* CACNGAPPLET_H */
