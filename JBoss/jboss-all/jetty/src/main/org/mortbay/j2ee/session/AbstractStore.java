// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AbstractStore.java,v 1.1.2.4 2003/07/30 23:18:19 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.util.Timer;
import java.util.TimerTask;
import javax.servlet.http.HttpServletRequest;
import org.jboss.logging.Logger;

//----------------------------------------

public abstract class
  AbstractStore
  implements Store
{
  protected final static Logger _log=Logger.getLogger(AbstractStore.class);

  // distributed scavenging

  /**
   * The period between scavenges
   */
  protected int _scavengerPeriod=60*30;	// 1/2 an hour
  public int getScavengerPeriod() {return _scavengerPeriod;}
  public void setScavengerPeriod(int secs) {_scavengerPeriod=secs;}
  /**
   * The extra time we wait before tidying up a CMPState to ensure
   * that if it loaded locally it will be scavenged locally first...
   */
  protected int _scavengerExtraTime=60*30; // 1/2 an hour
  public int getScavengerExtraTime() {return _scavengerExtraTime;}
  public void setScavengerExtraTime(int secs) {_scavengerExtraTime=secs;}
  /**
   * A maxInactiveInterval of -1 means never scavenge. The DB would
   * fill up very quickly - so we can override -1 with a real value
   * here.
   */
  protected int _actualMaxInactiveInterval=60*60*24*28;	// 28 days
  public int getActualMaxInactiveInterval() {return _actualMaxInactiveInterval;}
  public void setActualMaxInactiveInterval(int secs) {_actualMaxInactiveInterval=secs;}

  public Object
    clone()
    {
      try
      {
	AbstractStore as=(AbstractStore)(getClass().newInstance());
	as.setScavengerPeriod(_scavengerPeriod);
	as.setScavengerExtraTime(_scavengerExtraTime);
	as.setActualMaxInactiveInterval(_actualMaxInactiveInterval);
	return as;
      }
      catch (Exception e)
      {
	_log.warn("could not clone Store", e);
	return null;
      }
    }

  protected Manager _manager;
  public Manager getManager(){return _manager;}
  public void setManager(Manager manager){_manager=manager;}


  protected Timer _scavenger;

  // Store LifeCycle
  public void
    start()
    throws Exception
    {
      _log.trace("starting...");
      boolean isDaemon=true;
      _scavenger=new Timer(isDaemon);
      long delay=_scavengerPeriod+Math.round(Math.random()*_scavengerPeriod);
      if (_log.isDebugEnabled()) _log.debug("starting distributed scavenger thread...(period: "+delay+" secs)");
      _scavenger.scheduleAtFixedRate(new Scavenger(), delay*1000, _scavengerPeriod*1000);
      _log.debug("...distributed scavenger thread started");
      _log.trace("...started");
    }

  public void
    stop()
    {
      _log.trace("stopping...");
      _log.debug("stopping distributed scavenger thread...");
      _scavenger.cancel();
      _scavenger=null;
      _log.debug("...distributed scavenger thread stopped");

      try
      {
	scavenge();
      }
      catch (Exception e)
      {
	_log.warn("error scavenging distributed sessions", e);
      }

      _log.trace("...stopped");
    }

  public void
    destroy()
    {
      _log.trace("destroying...");
      _log.trace("...destroyed");
    }

  class Scavenger
    extends TimerTask
  {
    public void
      run()
    {
      try
      {
	scavenge();
      }
      catch (Exception e)
      {
	_log.warn("could not scavenge distributed sessions", e);
      }
    }
  }

  // ID allocation

  public String
    allocateId(HttpServletRequest request)
    {
      return getManager().getIdGenerator().nextId(request);
    }

  public void
    deallocateId(String id)
    {
      // these ids are disposable
    }

  // still abstract...

  //   // Store LifeCycle
  //   void destroy();	// corresponds to ctor
  //
  //   boolean isDistributed();
  //
  //   // State LifeCycle
  //   State newState(String id, int maxInactiveInterval) throws Exception;
  //   State loadState(String id) throws Exception;
  //   void  storeState(State state) throws Exception;
  //   void  removeState(State state) throws Exception;
  //
  //   // Store misc
  //   void scavenge() throws Exception;
  //   void passivateSession(StateAdaptor sa);
}
