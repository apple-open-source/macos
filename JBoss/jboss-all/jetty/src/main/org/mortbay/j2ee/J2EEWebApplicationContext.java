// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: J2EEWebApplicationContext.java,v 1.1.4.3 2003/02/16 01:16:10 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee;

import java.io.IOException;
import java.util.List;
import org.mortbay.jetty.servlet.SessionManager;
import org.mortbay.jetty.servlet.WebApplicationContext;
import org.mortbay.j2ee.session.Manager;
import org.apache.log4j.Category;

public class
  J2EEWebApplicationContext
  extends WebApplicationContext
{
  protected Category _log=Category.getInstance(getClass().getName());

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
      _log.debug("setDistributable "+distributable);
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
