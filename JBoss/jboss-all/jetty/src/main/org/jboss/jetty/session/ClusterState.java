
package org.jboss.jetty.session;

import java.util.Map;
import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;
import org.mortbay.j2ee.session.State;
import org.mortbay.j2ee.session.LocalState;

import java.rmi.RemoteException;
import java.util.Enumeration;
import java.util.Map;
import org.jboss.ha.httpsession.server.ClusteredHTTPSessionServiceMBean;
import org.jboss.logging.Logger;
import org.jboss.metadata.WebMetaData;

//----------------------------------------

class
  ClusterStateEnvelope
  implements State
{
  protected static final Logger                    _log=Logger.getLogger(ClusterStateEnvelope.class);
  protected final ClusteredHTTPSessionServiceMBean _svc;
  protected final String                           _id;

  ClusterStateEnvelope(ClusteredHTTPSessionServiceMBean svc, String id)
    {
      _svc=svc;
      _id=id;
    }

  State
    loadState(String id)
    {
      State state=null;
      try
      {
         ClassLoader tcl = Thread.currentThread().getContextClassLoader();
	state=(State)_svc.getHttpSession(id, tcl);

	if (_log.isDebugEnabled()) _log.debug("loading ClusterState: "+id+"/"+state);
	//	state.setId(id);

	if (_log.isDebugEnabled()) _log.debug("found ClusterState: "+id);
      }
      catch (Throwable ignore)
      {
	//	_log.warn("no such HttpSession: "+id);
      }

      return state;
    }

  void
    storeState(String id, State state)
    {
      ClusterState cs=(ClusterState)state;

      if (_log.isDebugEnabled()) _log.debug("storing ClusterState: "+id+"/"+state);

      if (_log.isDebugEnabled()) _log.debug("distributing ClusterState: "+id);

      _svc.setHttpSession(id, cs);
    }

  // State API

  // cached/invariant
  public String
    getId()
    throws RemoteException
    {
      return _id;
    }

  // readers
  public long
    getCreationTime()
    throws RemoteException
    {
      return loadState(_id).getCreationTime();
    }

  public int
    getActualMaxInactiveInterval()
    throws RemoteException
    {
      return loadState(_id).getActualMaxInactiveInterval();
    }

  public long
    getLastAccessedTime()
    throws RemoteException
    {
      return loadState(_id).getLastAccessedTime();
    }

  public int
    getMaxInactiveInterval()
    throws RemoteException
    {
      return loadState(_id).getMaxInactiveInterval();
    }

  public Object
    getAttribute(String name)
    throws RemoteException
    {
      return loadState(_id).getAttribute(name);
    }

  public void
    setLastAccessedTime(long time)
    throws RemoteException
    {
    }

  public Map
    getAttributes()
    throws RemoteException
    {
      return loadState(_id).getAttributes();
    }

  public Enumeration
    getAttributeNameEnumeration()
    throws RemoteException
    {
      return loadState(_id).getAttributeNameEnumeration();
    }

  public String[]
    getAttributeNameStringArray()
    throws RemoteException
    {
      return loadState(_id).getAttributeNameStringArray();
    }

  public boolean
    isValid()
    throws RemoteException
    {
      return loadState(_id).isValid();
    }

  // writers

  public void
    setMaxInactiveInterval(int interval)
    throws RemoteException
    {
      State state=loadState(_id);
      state.setMaxInactiveInterval(interval);
      storeState(_id,state);
    }

  public Object
    setAttribute(String name, Object value, boolean returnValue)
    throws RemoteException
    {
      State state=loadState(_id);
      Object tmp=state.setAttribute(name, value, returnValue);
      storeState(_id, state);
      return tmp;
    }

  public Object
    removeAttribute(String name, boolean returnValue)
    throws RemoteException
    {
      State state=loadState(_id);
      Object tmp=state.removeAttribute(name, returnValue);
      storeState(_id, state);
      return tmp;
    }

  public void
    setAttributes(Map attributes)
    throws RemoteException
    {
      State state=loadState(_id);
      state.setAttributes(attributes);
      storeState(_id, state);
    }
}

//----------------------------------------

public class ClusterState
  extends LocalState
  implements SerializableHttpSession
{
  public
    ClusterState(String id, int maxInactiveInterval, int actualMaxInactiveInterval)
  {
    super(id, maxInactiveInterval, actualMaxInactiveInterval);
  }

  public
    ClusterState()
  {
    // USE FOR DESERIALISING ONLY
  }

  // hack
  public void
    setId(String id)
  {
    _id=id;
  }

  // SerializableHttpSession API
  public boolean
    areAttributesModified(SerializableHttpSession previous)
  {
    // I'm not going to try to do a comparison - it could be very
    // costly/impossible - our algorithm will have to find other means
    // of optimisation...
    return true;
  }

  public long
    getContentCreationTime()
  {
    return getCreationTime();
  }

  public long
    getContentLastAccessTime()
  {
    return getLastAccessedTime();
  }

   public void sessionHasBeenStored()
   {
   }

   /**
    * is the session replicated sync- or asynchronously
    *
    * @see  WebMetaData
    */
   public int getReplicationTypeForSession()
   {
      // defensive: NYI
      return WebMetaData.REPLICATION_TYPE_SYNC;
   }
}
