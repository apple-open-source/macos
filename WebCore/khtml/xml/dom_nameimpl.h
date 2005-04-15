/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef _DOM_NameImpl_h_
#define _DOM_NameImpl_h_

#include "dom/dom_atomicstring.h"

namespace DOM {

class Name {
public:
    Name() {};
    Name(const AtomicString& namespaceURI, const AtomicString& localName)
        :m_namespaceURI(namespaceURI), m_localName(localName) {}

    const AtomicString& namespaceURI() const { return m_namespaceURI; }
    const AtomicString& localName() const { return m_localName; }

private:
    AtomicString m_namespaceURI;
    AtomicString m_localName;
    
    friend bool operator==(const Name& a, const Name& b);
};

inline bool operator==(const Name& a, const Name& b)
{
    return a.m_namespaceURI == b.m_namespaceURI && a.m_localName == b.m_localName;
}

inline bool operator!=(const Name& a, const Name& b)
{
    return !(a == b);
}

}
#endif
