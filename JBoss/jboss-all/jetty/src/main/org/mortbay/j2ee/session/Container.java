// Container is Object at front of stack
// at beginning of call, sets up threadlocals
// Interceptors divided into Stateful/less Interceptors
//// Interceptos implement cloneable

package org.mortbay.j2ee.session;

import java.util.ListIterator;
import java.util.ArrayList;
import java.util.List;
import org.apache.log4j.Category;
import javax.servlet.http.HttpSession;

public class
    Container
    extends ArrayList
    implements Cloneable
{
  Category _log=Category.getInstance(getClass().getName());

// this will come into service when I figure out how to remove the
// next interceptor from each interceptor's state...

//   public Object
//     clone()
//     {
//       Container c=new Container();
//
//       for (Iterator i=iterator(); i.hasNext();)
// 	c.add(((StateInterceptor)i.next()).clone());
//
//       return c;
//     }

  public Object
    clone()
    {
      Container c=new Container();

      try
      {
	State state=null;

	for (ListIterator i=listIterator(size()); i.hasPrevious();)
	{
	  State lastState=state;
	  StateInterceptor si=(StateInterceptor)i.previous();
	  si=(StateInterceptor)si.getClass().newInstance();
	  si.setState(lastState);
	  state=si;
	  c.add(0,state);
	}
      }
      catch (Exception e)
      {
	_log.error("could not clone Container", e);
      }

      return c;
    }

  // newContainer(this, id, state, getMaxInactiveInterval(), currentSecond()

  public static HttpSession
    newContainer(Manager manager, String id, State state, int maxInactiveInterval, long currentSecond, StateInterceptor[] interceptors)
    {
      // put together the make-believe container and HttpSession state

      StateAdaptor adp=new StateAdaptor(id, manager, maxInactiveInterval, currentSecond);

      State last=state;
      try
      {
	Class[] ctorParams={};
	for (int i=interceptors.length; i>0; i--)
	{
	  StateInterceptor si=interceptors[i-1];
//	  _log.debug("adding interceptor instance: "+name);
	  StateInterceptor interceptor=(StateInterceptor)si.clone();
	  si.setManager(manager); // overkill - but safe
	  si.setSession(adp);	// overkill - but safe
	  interceptor.setState(last); // this is also passed into ctor - make up your mind - TODO
	  interceptor.start();
	  last=interceptor;
	}
      }
      catch (Exception e)
      {
	//	_log.error("could not build distributed HttpSession container", e);
      }

      adp.setState(last);

      return adp;
    }

  public static State
    destroyContainer(HttpSession session, StateInterceptor[] interceptors)
  {
    // dissasemble the container here to aid GC

    StateAdaptor sa=(StateAdaptor)session;
    State last=sa.getState(); sa.setState(null);

    for (int i=interceptors.length; i>0; i--)
    {
      StateInterceptor si=(StateInterceptor)last;
      si.stop();
      State s=si.getState();
      si.setState(null);
      last=s;
    }

    return last;
  }
}
