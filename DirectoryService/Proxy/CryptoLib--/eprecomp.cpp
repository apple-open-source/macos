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
#include "pch.h"
#include "eprecomp.h"

NAMESPACE_BEGIN(CryptoPP)

template <class T> void ExponentiationPrecomputation<T>::SetGroupAndBase(const Group &group, const Element &base)
{
	m_group = &group;
	m_bases.resize(1);
	m_bases[0] = base;
}

template <class T> void ExponentiationPrecomputation<T>::Precompute(unsigned int maxExpBits, unsigned int storage)
{
	assert(m_group != NULL);
	assert(m_bases.size() > 0);
	assert(storage <= maxExpBits);

	if (storage > 1)
	{
		m_windowSize = (maxExpBits+storage-1)/storage;
		m_exponentBase = Integer::Power2(m_windowSize);
	}

	m_bases.resize(storage);
	for (unsigned i=1; i<storage; i++)
		m_bases[i] = m_group->ScalarMultiply(m_bases[i-1], m_exponentBase);
}

template <class T> void ExponentiationPrecomputation<T>::PrepareCascade(std::vector<BaseAndExponent<Element> > &eb, const Integer &exponent) const
{
	Integer r, q, e = exponent;
	bool fastNegate = m_group->InversionIsFast() && m_windowSize > 1;
	unsigned int i;

	for (i=0; i+1<m_bases.size(); i++)
	{
		Integer::DivideByPowerOf2(r, q, e, m_windowSize);
		std::swap(q, e);
		if (fastNegate && r.GetBit(m_windowSize-1))
		{
			++e;
			eb.push_back(BaseAndExponent<Element>(m_group->Inverse(m_bases[i]), m_exponentBase - r));
		}
		else
			eb.push_back(BaseAndExponent<Element>(m_bases[i], r));
	}
	eb.push_back(BaseAndExponent<Element>(m_bases[i], e));
}

template <class T> ExponentiationPrecomputation<T>::Element ExponentiationPrecomputation<T>::Exponentiate(const Integer &exponent) const
{
	std::vector<BaseAndExponent<Element> > eb;	// array of segments of the exponent and precalculated bases
	eb.reserve(m_bases.size());
	PrepareCascade(eb, exponent);
	return GeneralCascadeMultiplication<Element>(*m_group, eb.begin(), eb.end());
}

template <class T> T 
	ExponentiationPrecomputation<T>::CascadeExponentiate(const Integer &exponent, 
		const ExponentiationPrecomputation<T> &pc2, const Integer &exponent2) const
{
	std::vector<BaseAndExponent<Element> > eb;	// array of segments of the exponent and precalculated bases
	eb.reserve(m_bases.size() + pc2.m_bases.size());
	PrepareCascade(eb, exponent);
	pc2.PrepareCascade(eb, exponent2);
	return GeneralCascadeMultiplication<Element>(*m_group, eb.begin(), eb.end());
}

NAMESPACE_END
