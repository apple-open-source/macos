// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: IdGenerator.java,v 1.1.2.1 2003/07/30 23:18:19 jules_gosnell Exp $
// ========================================================================

/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.mortbay.j2ee.session;

import javax.servlet.http.HttpServletRequest;

public interface
  IdGenerator
  extends Cloneable
{
  public Object clone();
  public String nextId(HttpServletRequest request);
}
