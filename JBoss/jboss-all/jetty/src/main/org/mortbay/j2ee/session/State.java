// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: State.java,v 1.1.4.1 2002/08/24 18:53:36 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import java.util.Enumeration;
import java.util.Map;

//----------------------------------------

// The API around the isolated state encapsulated by an HttpSession -
// NOT quite the same as an HttpSession interface...

// It would be much cheaper to have set/removeAttribute return a
// boolean or void - but we HAVE TO HAVE the old binding to use in
// ValueUnbound events...

//----------------------------------------

/**
 * Implemented by objects wishing to be used to store the state from
 * an HttpSession.
 *
 * @author <a href="mailto:jules@mortbay.com">Jules Gosnell</a>
 * @version 1.0
 */
public interface
  State
{
  // invariant field accessors
  String      getId()                                                      throws RemoteException;
  int         getActualMaxInactiveInterval()                               throws RemoteException;
  long        getCreationTime()                                            throws RemoteException;

  // variant field accessors
  Map         getAttributes()                                              throws RemoteException;
  void        setAttributes(Map attributes)                                throws RemoteException;
  long        getLastAccessedTime()                                        throws RemoteException;
  void        setLastAccessedTime(long time)                               throws RemoteException;
  int         getMaxInactiveInterval()                                     throws RemoteException;
  void        setMaxInactiveInterval(int interval)                         throws RemoteException;

  // compound fn-ality
  Object      getAttribute(String name)                                    throws RemoteException;
  Object      setAttribute(String name, Object value, boolean returnValue) throws RemoteException;
  Object      removeAttribute(String name, boolean returnValue)            throws RemoteException;
  Enumeration getAttributeNameEnumeration()                                throws RemoteException;
  String[]    getAttributeNameStringArray()                                throws RemoteException;
  boolean     isValid()                                                    throws RemoteException;
}

