// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: JGStore.java,v 1.1.2.17 2003/07/11 14:27:46 gregwilkins Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------
import java.io.IOException;
import java.lang.reflect.Method;
import java.rmi.RemoteException;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Vector;
import org.apache.log4j.Category;
import org.javagroups.Address;
import org.javagroups.Channel;
import org.javagroups.JChannel;
import org.javagroups.MembershipListener; // we are notified of changes to membership list
import org.javagroups.MergeView;
import org.javagroups.Message;
import org.javagroups.MessageListener; // we are notified of changes to other state
import org.javagroups.View;
import org.javagroups.blocks.GroupRequest;
import org.javagroups.blocks.MessageDispatcher;
import org.javagroups.blocks.MethodCall;
import org.javagroups.blocks.RpcDispatcher;
import org.javagroups.util.Util;

//----------------------------------------

// what happens if a member drops away for a while then comes back -
// can we deal with it ?

// quite a lot left to do:

// how do we bring ourselves or others up to date on startup whilst
// not missing any updates ? - talk to Bela

//how do we avoid the deserialisation cost like Sacha - store updates
//serialised and deserialise lazily (we would need a custom class so
//we don't get confused by a user storing their own Serialised objects
//?

// Talk to Sacha...

// It will be VERY important that nodes using this Store have their clocks synched...

/**
 * publish changes to our state, receive and dispatch notification of
 * changes in other states, initialise our state from other members,
 * allow other members to initialise their state from us - all via
 * JavaGroups...
 *
 * @author <a href="mailto:jules@mortbay.com">Jules Gosnell</a>
 * @version 1.0
 */
public class
  JGStore
  extends AbstractReplicatedStore
  implements MessageListener, MembershipListener
{
  protected String _protocolStack=""+
    "UDP(mcast_addr=228.8.8.8;mcast_port=45566;ip_ttl=32;" +
    "ucast_recv_buf_size=16000;ucast_send_buf_size=16000;" +
    "mcast_send_buf_size=32000;mcast_recv_buf_size=64000;loopback=true):"+
    "PING(timeout=2000;num_initial_members=3):"+
    "MERGE2(min_interval=5000;max_interval=10000):"+
    "FD_SOCK:"+
    "VERIFY_SUSPECT(timeout=1500):"+
    "pbcast.STABLE(desired_avg_gossip=20000):"+
    "pbcast.NAKACK(gc_lag=50;retransmit_timeout=300,600,1200,2400,4800;max_xmit_size=8192):"+
    "UNICAST(timeout=2000):"+
    "FRAG(frag_size=8192;down_thread=false;up_thread=false):"+
    "pbcast.GMS(join_timeout=5000;join_retry_timeout=2000;shun=false;print_local_addr=true):"+
    "pbcast.STATE_TRANSFER";
  public String getProtocolStack() {return _protocolStack;}
  public void setProtocolStack(String protocolStack) {_protocolStack=protocolStack;}

  protected String _subClusterName="DefaultSubCluster";
  public String getSubClusterName() {return _subClusterName;}
  public void setSubClusterName(String subClusterName) {_subClusterName=subClusterName;}

  protected int _retrievalTimeOut=20000; // 20 seconds
  public int getRetrievalTimeOut() {return _retrievalTimeOut;}
  public void setRetrievalTimeOut(int retrievalTimeOut) {_retrievalTimeOut=retrievalTimeOut;}

  protected int _distributionModeInternal=GroupRequest.GET_ALL; // synchronous/non-sticky
  protected int getDistributionModeInternal() {return _distributionModeInternal;}
  protected void
    setDistributionModeInternal(String distributionMode)
    {
      try
      {
	_distributionModeInternal=GroupRequest.class.getDeclaredField(distributionMode).getInt(GroupRequest.class);
      }
      catch (Exception e)
      {
	_log.error("could not convert "+distributionMode+" to GroupRequest field", e);
      }
      _log.debug("GroupRequest:"+distributionMode+"="+_distributionModeInternal);
    }

  protected String _distributionMode="GET_ALL"; // synchronous/non-sticky
  public String getDistributionMode() {return _distributionMode;}
  public void
    setDistributionMode(String distributionMode)
    {
      _distributionMode=distributionMode;
      setDistributionModeInternal(_distributionMode);
    }

  protected int _distributionTimeOut=5000;	// 5 seconds
  public int getDistributionTimeOut() {return _distributionTimeOut;}
  public void setDistributionTimeOut(int distributionTimeOut) {_distributionTimeOut=distributionTimeOut;}

  public Object
    clone()
    {
      JGStore jgs=(JGStore)super.clone();
      jgs.setProtocolStack(getProtocolStack());
      jgs.setSubClusterName(getSubClusterName());
      jgs.setRetrievalTimeOut(getRetrievalTimeOut());
      jgs.setDistributionMode(getDistributionMode());
      jgs.setDistributionTimeOut(getDistributionTimeOut());

      return jgs;
    }

  //----------------------------------------

  protected Channel       _channel;
  protected RpcDispatcher _dispatcher;
  protected Vector        _members;

  //----------------------------------------
  // Store API - Store LifeCycle

  protected void
    init()
    {
      try
      {
	// start up our channel...
	_channel=new JChannel(getProtocolStack()); // channel should be JBoss or new Jetty channel

	MessageListener messageListener=this;
	MembershipListener membershipListener=this;
	Object serverObject=this;
	_dispatcher=new RpcDispatcher(_channel, messageListener, membershipListener, serverObject);
	_dispatcher.setMarshaller(new RpcDispatcher.Marshaller() {
	    public Object
	      objectFromByteBuffer(byte[] buf)
	    {
	      ClassLoader oldLoader=Thread.currentThread().getContextClassLoader();
	      try
	      {
		Thread.currentThread().setContextClassLoader(getLoader());
		return MarshallingInterceptor.demarshal(buf);
	      }
	      catch (Exception e)
	      {
		_log.error("could not demarshal incoming update", e);
	      }
	      finally
	      {
		Thread.currentThread().setContextClassLoader(oldLoader);
	      }
	      return null;
	    }

	    public byte[]
	      objectToByteBuffer(Object obj)
	    {
	      try
	      {
		return MarshallingInterceptor.marshal(obj);
	      }
	      catch (Exception e)
	      {
		_log.error("could not marshal outgoing update", e);
	      }
	      return null;
	    }
	  });

	_channel.setOpt(Channel.GET_STATE_EVENTS, Boolean.TRUE);

	View view=_channel.getView();
	if (view!=null)
	  _members=(Vector)view.getMembers().clone();

	if (_members!=null)
	{
	  _members=(Vector)_members.clone(); // we don't own it
	  _members.remove(_channel.getLocalAddress());
	}
	else
	  _members=new Vector(0);

	// _log.info("current view members: "+_members);
      }
      catch (Exception e)
      {
	_log.error("could not initialise JavaGroups Channel and Dispatcher", e);
      }
    }

  public String
    getChannelName()
    {
      return "JETTY_HTTPSESSION_DISTRIBUTION:"+getContextPath()+"-"+getSubClusterName();
    }

  public void
    start()
    throws Exception
    {
      super.start();

      init();

      String channelName=getChannelName();
      _log.debug("starting ("+channelName+")....");

      _channel.connect(channelName); // group should be on a per-context basis
      _dispatcher.start();

      if (!_channel.getState(null, getRetrievalTimeOut()))
	_log.info("could not retrieve current sessions from JavaGroups - assuming this to be initial node");

      _log.debug("started ("+channelName+")....");
    }

  public void
    stop()
    {
      _timer.cancel();
      _dispatcher.stop();
      _channel.disconnect();

      super.stop();
    }

  public void
    destroy()
    {
      _timer=null;
      _dispatcher=null;
      _channel=null;

      super.destroy();
    }

  //----------------------------------------
  // AbstractReplicatedStore API



  protected Object _idsLock=new Object();
  protected Set _ids=new HashSet();
  protected Timer _timer=new Timer();
  protected long  _period=0;
  protected TimerTask _task=new TouchTimerTask();

  protected class TouchTimerTask extends TimerTask
  {
    protected Set _oldIds=null;
    protected Set _newIds=new HashSet();

    public void
      run()
    {
      synchronized (_idsLock)
      {
	_oldIds=_ids;
	_ids=_newIds;		// empty
	_newIds=null;
      }

      //      _log.info("TOUCHING SESSIONS: "+_oldIds);
      publish(null, TOUCH_SESSIONS, new Object[] {_oldIds.toArray(new String[_oldIds.size()]), new Long(System.currentTimeMillis()+_period)});
      _oldIds.clear();
      _newIds=_oldIds;		// recycle Set for next distribution
      _oldIds=null;
    }
  }


  public long getBatchPeriod(){return _period;}
  public void setBatchPeriod(long period){_period=period;}

  protected void
    publish(String id, Method method, Object[] argInstances)
    {
      //      _log.info("publishing: "+id+" - "+methodName);

      if (_period>0)
      {
	if (method.equals(SET_LAST_ACCESSED_TIME))
	{
	  // push into set to be touched when a timer expires...
	  synchronized (_idsLock)
	  {
	    // kick off timer as soon as something that needs publishing
	    // appears...
	    if (_ids.size()==0)
	    {
	      _timer.schedule(new TouchTimerTask(), _period); // TODO - reuse old task
	      _log.info("scheduling timer...");
	    }

	    _ids.add(id);
	  };
	  return;
	}
	else if (method.equals(DESTROY_SESSION))
	{
	  String tmp=(String)argInstances[0]; // id in factory methods
	  //	  System.out.println("LOCAL DESTRUCTION : "+tmp); // arg[0] is the id
	  // this session has been destroyed locally...
	  synchronized (_idsLock)
	  {
	    _ids.remove(tmp);
	  }
	}
      }

      try
      {
	Class[] tmp={String.class, Integer.class, Object[].class};
	MethodCall mc = new MethodCall(getClass().getMethod("dispatch",tmp));
	mc.addArg(id);
	mc.addArg(_methodToInteger.get(method.getName()));
	mc.addArg(argInstances);

	// we need some way of synchronising _member read/write-ing...
	_dispatcher.callRemoteMethods(_members,
				      mc,
				      getDistributionModeInternal(),
				      getDistributionTimeOut());
      }
      catch(Exception e)
      {
	_log.error("problem publishing change in state over JavaGroups", e);
      }

    }

  // JG doesn't find this method in our superclass ...
  public void
    dispatch(String id, Integer method, Object[] argInstances)
    {
      //      System.out.println("REMOTE INVOCATION : "+_integerToMethod[method.intValue()].getName()+" : "+id);
      if (_integerToMethod[method.intValue()].equals(DESTROY_SESSION))
      {
	String tmp=(String)argInstances[0]; // id in factory methods
	//	System.out.println("REMOTE DESTRUCTION : "+tmp); // arg[0] is the id
	// this session has been destroyed remotely...
	synchronized (_idsLock)
	{
	  _ids.remove(tmp);
	}
      }

      //      _log.info("dispatching: "+id+" - "+methodName);

      // we should check the context name here, or not bother sending it...

      ClassLoader oldLoader=Thread.currentThread().getContextClassLoader();
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

  //----------------------------------------
  // 'MessageListener' API

  /**
   * receive notification of someone else's change in state
   *
   * @param msg a <code>Message</code> value
   */
  public void
    receive(Message msg)
    {
      //      _log.info("**************** RECEIVE CALLED *********************");
      byte[] buf=msg.getBuffer();
    }

  /**
   * copy our state to be used to initialise another store...
   *
   * @return an <code>Object</code> value
   */

  // should we cache the state, in case two new nodes come up together ?

  public synchronized byte[]
			    getState()
    {
      ClassLoader oldLoader=Thread.currentThread().getContextClassLoader();
      try
      {
	Thread.currentThread().setContextClassLoader(getLoader());
	_log.info("initialising another store from our current state");

	// this is a bit problematic - since we really need to freeze
	// every session before we can dump them... - TODO
	LocalState[] state;
	synchronized (_sessions)
	{
	  _log.info("sending "+_sessions.size()+" sessions");

	  state=new LocalState[_sessions.size()];
	  int j=0;
	  for (Iterator i=_sessions.values().iterator(); i.hasNext();)
	    state[j++]=(LocalState)i.next();
	}

	Object[] data={new Long(System.currentTimeMillis()), state};
	try
	{
	  return MarshallingInterceptor.marshal(data);
	}
	catch (Exception e)
	{
	  _log.error ("Unable to getState from JavaGroups: ", e);
	  return null;
	}
      }
      finally
      {
	Thread.currentThread().setContextClassLoader(oldLoader);
      }
    }

  /**
   * initialise ourself from the current state of another store...
   *
   * @param new_state an <code>Object</code> value
   */
  public synchronized void
    setState (byte[] tmp)
    {
      if (tmp!=null)
      {
	_log.info("initialising our state from another Store");

	Object[] data = null;
	try
	{
	  // TODO - this needs to be loaded into webapps ClassLoader,
	  // then we can lose the MarshallingInterceptor...
	  data=(Object[])MarshallingInterceptor.demarshal(tmp);
	}
	catch (Exception e)
	{
	  _log.error ("Unable to setState from JavaGroups: ", e);
	}

	AbstractReplicatedStore.setReplicating(true);

	long remoteTime=((Long)data[0]).longValue();
	long localTime=System.currentTimeMillis();
	long disparity=(localTime-remoteTime)/1000;
	_log.info("time disparity: "+disparity+" secs");

	LocalState[] state=(LocalState[])data[1];
	_log.info("receiving "+state.length+" sessions...");

	for (int i=0; i<state.length; i++)
	{
	  LocalState ls=state[i];
	  _sessions.put(ls.getId(), ls);
	  getManager().getHttpSession(ls.getId()); // should cause creation of corresponding InterceptorStack
	}
      }
    }

  //----------------------------------------
  // 'MembershipListener' API

  // Block sending and receiving of messages until viewAccepted() is called
  public void
    block()
    {
      _log.info("block()");
    }

  // Called when a member is suspected
  public synchronized void
    suspect(Address suspected_mbr)
    {
      _log.info("suspect("+suspected_mbr+")");
    }

  // Called when channel membership changes
  public synchronized void
    viewAccepted(View newView)
    {
      _log.info("viewAccepted("+newView+")");

      boolean isMerge=(newView instanceof MergeView);

      if(isMerge)
	_log.warn("merging... NYI: merge view is " + newView);

      Vector newMembers=newView.getMembers();

      if (newMembers != null)
      {
 	_members.clear();
 	_members.addAll(newMembers);
	_members.remove(_channel.getLocalAddress());
	_log.info("current view members: "+_members);
      }
    }
}
