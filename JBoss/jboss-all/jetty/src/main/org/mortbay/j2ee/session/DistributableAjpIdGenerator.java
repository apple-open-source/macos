// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: DistributableAjpIdGenerator.java,v 1.1.2.1 2003/07/30 23:18:19 jules_gosnell Exp $
// ========================================================================

/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.mortbay.j2ee.session;

import javax.servlet.http.HttpServletRequest;

public class
  DistributableAjpIdGenerator
  extends DistributableIdGenerator
{
  public synchronized Object
    clone()
    {
      DistributableAjpIdGenerator daig=(DistributableAjpIdGenerator)super.clone();
      daig.setWorkerName(getWorkerName());
      return daig;
    }

  protected String _workerName;
  public String getWorkerName() { return _workerName; }
  public void setWorkerName(String workerName) { _workerName=workerName; }

  public String
    nextId(HttpServletRequest request)
    {
      String id=super.nextId(request);
      String s=(_workerName!=null)?_workerName:(String)request.getAttribute("org.mortbay.http.ajp.JVMRoute");
      return (s==null)?id:id+"."+s;
    }
}
