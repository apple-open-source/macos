// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: DelayInterceptor.java,v 1.1.4.2 2002/11/16 21:58:58 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import javax.servlet.http.HttpSession;

//----------------------------------------

// We need to ensure that calls to the HttpSession implementation are
// made in Jetty's and not the User's Transaction Context. Otherwise
// if their transaction is rolledback, our state is lost and
// vice-versa...

public class DelayInterceptor
  extends AroundInterceptor
{
  protected void
    before()
  {
    try
    {
      Thread.sleep(1000);
    }
    catch (Exception e)
    {
    }
  }

  protected void
    after()
  {
  }

  //  public Object clone() { return this; } // Stateless
}
