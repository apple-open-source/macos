/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef CRYPTOPP_MODARITH_H
#define CRYPTOPP_MODARITH_H

// implementations are in integer.cpp

#include "cryptopp.h"
#include "cryptopp_misc.h"
#include "integer.h"
#include "algebra.h"

NAMESPACE_BEGIN(CryptoPP)

//! .
class ModularArithmetic : public AbstractRing<Integer>
{
public:

	typedef int RandomizationParameter;
	typedef Integer Element;

	ModularArithmetic(const Integer &modulus = Integer::One())
		: modulus(modulus), result((word)0, modulus.reg.size) {}

	ModularArithmetic(const ModularArithmetic &ma)
		: modulus(ma.modulus), result((word)0, modulus.reg.size) {}

	ModularArithmetic(BufferedTransformation &bt);	// construct from BER encoded parameters

	void DEREncode(BufferedTransformation &bt) const;

	void DEREncodeElement(BufferedTransformation &out, const Element &a) const;
	void BERDecodeElement(BufferedTransformation &in, Element &a) const;

	const Integer& GetModulus() const {return modulus;}
	void SetModulus(const Integer &newModulus) {modulus = newModulus; result.reg.Resize(modulus.reg.size);}

	virtual Integer ConvertIn(const Integer &a) const
		{return a%modulus;}

	virtual Integer ConvertOut(const Integer &a) const
		{return a;}

	const Integer& Half(const Integer &a) const;

	bool Equal(const Integer &a, const Integer &b) const
		{return a==b;}

	const Integer& Zero() const
		{return Integer::Zero();}

	const Integer& Add(const Integer &a, const Integer &b) const;

	Integer& Accumulate(Integer &a, const Integer &b) const;

	const Integer& Inverse(const Integer &a) const;

	const Integer& Subtract(const Integer &a, const Integer &b) const;

	Integer& Reduce(Integer &a, const Integer &b) const;

	const Integer& Double(const Integer &a) const
		{return Add(a, a);}

	const Integer& One() const
		{return Integer::One();}

	const Integer& Multiply(const Integer &a, const Integer &b) const
		{return result1 = a*b%modulus;}

	const Integer& Square(const Integer &a) const
		{return result1 = a.Squared()%modulus;}

	bool IsUnit(const Integer &a) const
		{return Integer::Gcd(a, modulus).IsUnit();}

	const Integer& MultiplicativeInverse(const Integer &a) const
		{return result1 = a.InverseMod(modulus);}

	const Integer& Divide(const Integer &a, const Integer &b) const
		{return Multiply(a, MultiplicativeInverse(b));}

	Integer CascadeExponentiate(const Integer &x, const Integer &e1, const Integer &y, const Integer &e2) const;

	void SimultaneousExponentiate(Element *results, const Element &base, const Integer *exponents, unsigned int exponentsCount) const;

	unsigned int MaxElementBitLength() const
		{return (modulus-1).BitCount();}

	unsigned int MaxElementByteLength() const
		{return (modulus-1).ByteCount();}

	Element RandomElement( RandomNumberGenerator &rng , const RandomizationParameter &ignore_for_now = 0 ) const
		// left RandomizationParameter arg as ref in case RandomizationParameter becomes a more complicated struct
	{ 
		return Element( rng , Integer( (long) 0) , modulus - Integer( (long) 1 )   ) ; 
	}   

	static const RandomizationParameter DefaultRandomizationParameter ;

protected:
	Integer modulus;
	mutable Integer result, result1;

};

// const ModularArithmetic::RandomizationParameter ModularArithmetic::DefaultRandomizationParameter = 0 ;

//! do modular arithmetics in Montgomery representation for increased speed
class MontgomeryRepresentation : public ModularArithmetic
{
public:
	MontgomeryRepresentation(const Integer &modulus);	// modulus must be odd

	Integer ConvertIn(const Integer &a) const
		{return (a<<(WORD_BITS*modulus.reg.size))%modulus;}

	Integer ConvertOut(const Integer &a) const;

	const Integer& One() const
		{return result1 = Integer::Power2(WORD_BITS*modulus.reg.size)%modulus;}

	const Integer& Multiply(const Integer &a, const Integer &b) const;

	const Integer& Square(const Integer &a) const;

	const Integer& MultiplicativeInverse(const Integer &a) const;

	Integer CascadeExponentiate(const Integer &x, const Integer &e1, const Integer &y, const Integer &e2) const
		{return AbstractRing<Integer>::CascadeExponentiate(x, e1, y, e2);}

	void SimultaneousExponentiate(Element *results, const Element &base, const Integer *exponents, unsigned int exponentsCount) const
		{AbstractRing<Integer>::SimultaneousExponentiate(results, base, exponents, exponentsCount);}

private:
	Integer u;
	SecWordBlock workspace;
};

NAMESPACE_END

#endif
