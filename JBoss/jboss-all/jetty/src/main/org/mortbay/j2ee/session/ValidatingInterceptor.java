// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ValidatingInterceptor.java,v 1.1.2.2 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import javax.servlet.http.HttpSession;
import org.jboss.logging.Logger;

//----------------------------------------


public class ValidatingInterceptor
  extends AroundInterceptor
{
  protected static final Logger _log=Logger.getLogger(ValidatingInterceptor.class);

  protected void before() throws IllegalStateException {if (_running) checkValid();}
  protected void after() {}

  //----------------------------------------

  protected boolean _running=false;

  public void start() {_log.trace("start()");_running=true;}
  public void stop()  {_log.trace("stop()"); _running=false;}

  protected void
    checkValid()
    throws IllegalStateException
  {
    boolean valid=false;
    State state=getState();
    try
    {
      int mii=state.getMaxInactiveInterval(); // secs
      int keep=mii;
      mii=mii<1?getManager().getStore().getActualMaxInactiveInterval():mii; // secs
      long lat=state.getLastAccessedTime(); // milisecs
      long now=System.currentTimeMillis(); // milisecs

      int age=(int)((now-lat)/1000); // secs

      valid=(age<mii);
      if (_log.isTraceEnabled()) _log.trace("session keep="+keep+", mii="+mii+", lat="+lat+", now="+now+", age="+age+", valid="+valid);
    }
    catch (java.rmi.NoSuchObjectException ignore)
    {
      //      _log.info("IGNORE ABOVE NoSuchEntityException - harmless");
    }
    catch (javax.ejb.NoSuchEntityException ignore)
    {
      //      _log.info("IGNORE ABOVE NoSuchEntityException - harmless");
    }
    catch (Exception e)
    {
      _log.error("couldn't determine validity of HttpSession", e);
    }

    if (!valid)
      throw new IllegalStateException("invalid HttpSession - timed out");
  }

  //  public Object clone() { return this; } // Stateless
}
