// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: LocalState.java,v 1.1.4.7 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import org.jboss.logging.Logger;

//----------------------------------------

/**
 * Hold the state of an HttpSession
 *
 * @author <a href="mailto:jules@mortbay.com">Jules Gosnell</a>
 * @version 1.0
 */
public class
  LocalState
  implements State, java.io.Serializable
{
  protected static final Logger _log=Logger.getLogger(LocalState.class);

  protected String _id;
  protected int    _maxInactiveInterval;
  protected int    _actualMaxInactiveInterval;

  protected long   _creationTime;
  protected long   _lastAccessedTime;
  protected Map    _attributes;	// allocated lazily


  public
    LocalState(String id, long creationTime, int maxInactiveInterval, int actualMaxInactiveInterval)
    {
      _id=id;
      _creationTime=creationTime;
      _lastAccessedTime=_creationTime;
      _maxInactiveInterval=maxInactiveInterval;
      _actualMaxInactiveInterval=actualMaxInactiveInterval;
    }

  public
    LocalState(String id, int maxInactiveInterval, int actualMaxInactiveInterval)
    {
      _id=id;
      _creationTime=System.currentTimeMillis();
      _lastAccessedTime=_creationTime;
      _maxInactiveInterval=maxInactiveInterval;
      _actualMaxInactiveInterval=actualMaxInactiveInterval;
    }

  protected
    LocalState()
    {
      // for deserialisation only
    }

  public String      getId()                                 {return _id;}
  public long        getCreationTime()                       {return _creationTime;}
  public int         getActualMaxInactiveInterval()          {return _actualMaxInactiveInterval;}
  public long        getLastAccessedTime()                   {return _lastAccessedTime;}
  public void        setLastAccessedTime(long time)          {_lastAccessedTime=time;}
  public int         getMaxInactiveInterval()                {return _maxInactiveInterval;}
  public void        setMaxInactiveInterval(int interval)    {_log.info("_maxInactiveInterval="+interval);_maxInactiveInterval=interval;}

  // allocate attribute map lazily. This is more complex, but JSPs
  // seem to force allocation of sessions and then never put anything
  // in them! - so it is a worthwhile saving in speed and footprint...

  protected static Map         _emptyMap         =Collections.EMPTY_MAP;
  protected static Enumeration _emptyEnumeration =Collections.enumeration(Collections.EMPTY_LIST);
  protected static String[]    _emptyStringArray =new String[0]; // could this be changed by user ?

  protected void ensureAttributes() { if (_attributes==null) _attributes=new HashMap();}

  public Object
    getAttribute(String name)
    {
      //      _log.info("getAttribute("+name+")");
      return _attributes==null?null:_attributes.get(name);
    }

  public Map
    getAttributes()
    {
      return (_attributes==null || _attributes.size()==0)?_emptyMap:Collections.unmodifiableMap(_attributes);
    }

  public Enumeration
    getAttributeNameEnumeration()
    {
      return (_attributes==null || _attributes.size()==0)?_emptyEnumeration:Collections.enumeration(_attributes.keySet());
    }

  public String[]
    getAttributeNameStringArray()
    {
      return (_attributes==null || _attributes.size()==0)?_emptyStringArray:(String[])_attributes.keySet().toArray(new String[_attributes.size()]);
    }

  public Object
    setAttribute(String name, Object value, boolean returnValue)
    {
      // we can be sure that name is non-null, because this will have
      // been checked in our adaptor...

      //      _log.info("setAttribute("+name+", "+value+", "+returnValue+")");

      ensureAttributes();
      Object tmp=_attributes.put(name, value);
      return returnValue?tmp:null;
    }

  public void
    setAttributes(Map attributes)
    {
      if (_attributes!=null)
	_attributes.clear();

      if (attributes.size()>0)
      {
	ensureAttributes();
	_attributes.putAll(attributes);
      }
    }

  public Object
    removeAttribute(String name, boolean returnValue)
    {
      if (_attributes==null)
	return null;
      else
      {
	Object tmp=_attributes.remove(name);
	return returnValue?tmp:null;
      }
    }

  protected long
    remainingTime()
    {
      long maxInactiveInterval=_maxInactiveInterval<1?_actualMaxInactiveInterval:_maxInactiveInterval;
      return (_lastAccessedTime+(maxInactiveInterval*1000L))-System.currentTimeMillis();
    }

  public boolean
    isValid(int extraTime)
    {
      long remainingTime=remainingTime();
      long etime=(extraTime*1000L);
      boolean valid=remainingTime+etime>0;
      return valid;
    }

  public boolean
    isValid()
    {
      return isValid(0);
    }
}
