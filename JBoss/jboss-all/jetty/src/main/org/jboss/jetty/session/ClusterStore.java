package org.jboss.jetty.session;

//----------------------------------------

import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Set;
import java.util.Timer;
import java.util.TimerTask;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.ha.httpsession.server.ClusteredHTTPSessionServiceMBean;
import org.jboss.logging.Logger;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.mx.util.MBeanServerLocator;
import org.mortbay.j2ee.session.Manager;
import org.mortbay.j2ee.session.State;
import org.mortbay.j2ee.session.StateAdaptor;

//----------------------------------------

/**
 * A DistributedSession Store implemented on top of Sacha & Bill's
 * Clustering stuff...
 *
 * @author <a href="mailto:jules_gosnell@@yahoo.com">Jules Gosnell</a>
 * @version 1.0
 * @since 1.0
 */
public class ClusterStore
   extends org.mortbay.j2ee.session.AbstractReplicatedStore
{
  protected static final Logger _log = Logger.getLogger(ClusterStore.class);
  MBeanServer _server = null;
  ObjectName _name = null;
  ClusteredHTTPSessionServiceMBean _proxy;

  protected Manager _manager;
  public Manager getManager()
  {
    return _manager;
  }
  public void setManager(Manager manager)
  {
    _manager = manager;
  }

  // Store LifeCycle
  public void start() throws Exception
  {
    // we are only expecting one server...
    _server = MBeanServerLocator.locateJBoss();
    _name = new ObjectName("jboss", "service", "ClusteredHttpSession");
    _proxy =
      (ClusteredHTTPSessionServiceMBean) MBeanProxyExt.create(
							      ClusteredHTTPSessionServiceMBean.class,
							      _name);
    _log.info(
	      "Support for Cluster-based Distributed HttpSessions loaded successfully: "
	      + _name);
  }

  public void stop()
  {}

  public void destroy()
  {}

  // State LifeCycle
  public State newState(String id, int maxInactiveInterval)
  {
    return newState (id, maxInactiveInterval, 60 * 60 * 24);
  }

  protected State newState(String id, int maxInactiveInterval, int actualMaxInactiveInterval)
  {
    ClusterStateEnvelope env = new ClusterStateEnvelope(_proxy, id);
    ClusterState state =
      new ClusterState(id, maxInactiveInterval, actualMaxInactiveInterval);
    env.storeState(id, state);
    return env;
  }

  public State loadState(String id)
  {
    ClusterStateEnvelope env = new ClusterStateEnvelope(_proxy, id);
    if (env.loadState(id) != null) // could be static... - TODO
      return env;
    else
      return null;
  }

  public void storeState(State state)
  {
    // do nothing - it has already been done
  }

  public void removeState(State state)
  {
    String id = "<unknown>";
    try
    {
      id = state.getId();
      _proxy.removeHttpSession(id);

      if (_log.isDebugEnabled()) _log.debug("destroyed ClusterState: " + id);
    }
    catch (Throwable ignore)
    {
      _log.warn("removing unknown ClusterState: " + id);
    }
  }

  // ID allocation - we should use a decent ID allocation strategy here...
  public String allocateId()
  {
    String id = _proxy.getSessionId();

    if (_log.isDebugEnabled()) _log.debug("allocating distributed HttpSession id: " + id);

    return id;
  }

  public void deallocateId(String id)
  {
    // the ids are not reused
  }

  public boolean isDistributed()
  {
    return true;
  }

  public void scavenge()
  {
    // Sacha's stuff does this for us...
  }

  public void passivateSession(StateAdaptor sa)
  {
    // if it's in the store - it's already passivated...
  }

  // this stuff has not yet been plumbed in since the HA HttpSession
  // Service does not publish a rich enough API - it just seems to try
  // to clean up every 30 secs... - later
  protected int _scavengerPeriod = 60 * 30; // 1/2 an hour
  protected int _scavengerExtraTime = 60 * 30; // 1/2 an hour
  protected int _actualMaxInactiveInterval = 60 * 60 * 24 * 28; // 28 days

  public void setScavengerPeriod(int secs)
  {
    _scavengerPeriod = secs;
  }
  public void setScavengerExtraTime(int secs)
  {
    _scavengerExtraTime = secs;
  }
  public void setActualMaxInactiveInterval(int secs)
  {
    _actualMaxInactiveInterval = secs;
  }

  public Object clone()
  {
    ClusterStore cs = new ClusterStore();
    cs.setScavengerPeriod(_scavengerPeriod);
    cs.setScavengerExtraTime(_scavengerExtraTime);
    cs.setActualMaxInactiveInterval(_actualMaxInactiveInterval);

    return cs;
  }

  //----------------------------------------
  // AbstractReplicatedStore API

  protected Object _idsLock = new Object();
  protected Set _ids = new HashSet();
  protected Timer _timer = new Timer();
  protected long _period = 0;
  protected TimerTask _task = new TouchTimerTask();

  protected class TouchTimerTask extends TimerTask
  {
    protected Set _oldIds = null;
    protected Set _newIds = new HashSet();

    public void run()
    {
      synchronized (_idsLock)
      {
	_oldIds = _ids;
	_ids = _newIds; // empty
	_newIds = null;
      }

      //      _log.info("TOUCHING SESSIONS: "+_oldIds);
      publish(
	      null,
	      TOUCH_SESSIONS,
	      new Object[] {
		_oldIds.toArray(new String[_oldIds.size()]),
		new Long(System.currentTimeMillis() + _period)});
      _oldIds.clear();
      _newIds = _oldIds; // recycle Set for next distribution
      _oldIds = null;
    }
  }

  public long getBatchPeriod()
  {
    return _period;
  }
  public void setBatchPeriod(long period)
  {
    _period = period;
  }

  protected void publish(String id, Method method, Object[] argInstances)
  {
    //      _log.info("publishing: "+id+" - "+methodName);

    if (_period > 0)
    {
      if (method.equals(SET_LAST_ACCESSED_TIME))
      {
	// push into set to be touched when a timer expires...
	synchronized (_idsLock)
	{
	  // kick off timer as soon as something that needs publishing
	  // appears...
	  if (_ids.size() == 0)
	  {
	    _timer.schedule(new TouchTimerTask(), _period);
	    // TODO - reuse old task
	    _log.info("scheduling timer...");
	  }

	  _ids.add(id);
	};
	return;
      }
      else if (method.equals(DESTROY_SESSION))
      {
	String tmp = (String) argInstances[0]; // id in factory methods
	//    System.out.println("LOCAL DESTRUCTION : "+tmp); // arg[0] is the id
	// this session has been destroyed locally...
	synchronized (_idsLock)
	{
	  _ids.remove(tmp);
	}
      }
    }

    dispatch(
	     id,
	     (Integer) _methodToInteger.get(method.getName()),
	     argInstances);

  }

  // JG doesn't find this method in our superclass ...
  public void dispatch(String id, Integer method, Object[] argInstances)
  {
    //      System.out.println("REMOTE INVOCATION : "+_integerToMethod[method.intValue()].getName()+" : "+id);
    if (_integerToMethod[method.intValue()].equals(DESTROY_SESSION))
    {
      String tmp = (String) argInstances[0]; // id in factory methods
      synchronized (_idsLock)
      {
	_ids.remove(tmp);
      }
    }

    ClassLoader oldLoader = Thread.currentThread().getContextClassLoader();
    try
    {
      Thread.currentThread().setContextClassLoader(getLoader());
      super.dispatch(id, method, argInstances);
    }
    finally
    {
      Thread.currentThread().setContextClassLoader(oldLoader);
    }
  }

  public void createSession(
			    String id,
			    long creationTime,
			    int maxInactiveInterval,
			    int actualMaxInactiveInterval)
  {

    if (_log.isDebugEnabled()) _log.debug("creating replicated session: " + id);
    newState(id, maxInactiveInterval, actualMaxInactiveInterval);
  }

  public void destroySession(String id)
  {
    if (_log.isDebugEnabled()) _log.debug("destroying replicated session: " + id);
    try
    {
      _proxy.removeHttpSession(id);

      if (_log.isDebugEnabled()) _log.debug("destroyed ClusterState: " + id);
    }
    catch (Throwable ignore)
    {
      _log.warn("removing unknown ClusterState: " + id);
    }
  }

  public void touchSessions(String[] ids, long time)
  {
    // NYI
    //      _log.info("touching sessions...: "+ids);
    /*
      for (int i = 0; i < ids.length; i++)
      {
      String id = ids[i];
      _proxy.getHttpSession(id).XXX

      try
      {
      ((StateInterceptor) target).setLastAccessedTime(time);
      }
      catch (Exception e)
      {
      _log.warn(
      "unable to touch session: " + id + " probably already removed");
      }
      }
    */
  }

}
