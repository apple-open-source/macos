// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: J2EEWebApplicationContext.java,v 1.1.4.4 2003/07/26 11:49:40 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee;

import java.io.IOException;
import java.util.List;
import org.mortbay.jetty.servlet.SessionManager;
import org.mortbay.jetty.servlet.WebApplicationContext;
import org.mortbay.j2ee.session.Manager;
import org.jboss.logging.Logger;

public class
  J2EEWebApplicationContext
  extends WebApplicationContext
{
  protected static final Logger _log=Logger.getLogger(J2EEWebApplicationContext.class);

  public
    J2EEWebApplicationContext(String warUrl)
    throws IOException
    {
      super(warUrl);
    }

  //----------------------------------------------------------------------------
  // DistributedHttpSession support
  //----------------------------------------------------------------------------

  protected boolean _distributable=false;

  public boolean
    getDistributable()
    {
      return _distributable;
    }

  public void
    setDistributable(boolean distributable)
    {
      if (_log.isDebugEnabled()) _log.debug("setDistributable "+distributable);
      _distributable=distributable;
    }

  protected Manager _distributableSessionManager;

  public void
    setDistributableSessionManager(Manager manager)
    {
      //      _log.info("setDistributableSessionManager "+manager);
      _distributableSessionManager=(Manager)manager;
      _distributableSessionManager.setContext(this);
    }

  public Manager
    getDistributableSessionManager()
    {
      return _distributableSessionManager;
    }

  //----------------------------------------------------------------------------

  protected boolean _stopGracefully=false;

  public void
    setStopGracefully(boolean stopGracefully)
    {
      if (isStarted())
	throw new IllegalStateException("setStopGracefully() must be called before J2EEWebApplicationContext is started");

      _stopGracefully=stopGracefully;
    }

  public boolean getStopGracefully() {return _stopGracefully;}

  public void
    start()
    throws Exception
    {
      if (_stopGracefully && !getStatsOn())
	setStatsOn(true);

      super.start();
    }
}
