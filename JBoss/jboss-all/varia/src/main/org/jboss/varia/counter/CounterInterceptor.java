/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.counter;

import java.util.Map;

import javax.naming.InitialContext;
import javax.naming.NamingException;
import org.jboss.invocation.Invocation;
import org.jboss.ejb.Container;
import org.jboss.ejb.plugins.*;


import org.jboss.logging.Logger;
import org.jboss.varia.counter.CounterService;

/**
 * Interceptor that uses the CounterService MBean to record the length of time
 * spent in 'lower' interceptors (below it in the stack).
 * 
 * <p><b>How to use:</b></p>
 * <p>First, the CounterService MBean must be installed in JBoss.
 *    See counter-service.xml for details/examples.
 *
 * <p>Next, you need to configure this interceptor into the interceptor stacks
 * of any beans you wish to monitor. This can be done either globally for a
 * container-config in standardjboss.xml, or on a per-bean basis in a jar's
 * jboss.jcml. Just insert the following at the top of the &lt;container-interceptors&gt;
 * section. If you're overriding this for a bean in jboss.xml, you'll need to
 * override the entire container-interceptors section.</p>
 * 
 * <code>
 * &lt;interceptor&gt;org.jboss.varia.counter.CounterInterceptor&lt;/interceptor&gt;
 * </code>
 * 
 * <p>This can go anywhere in the container-interceptors section, but either
 * the top or the bottom will probably be best for gathering application
 * statistics.
 * 
 * @author <a href="mailto:danch@nvisia.com">Dan Christopherson</href>
 * @version $Revision: 1.2 $
 */
public class CounterInterceptor
   extends AbstractInterceptor
{
   Container container = null;
   CounterService counter = null;
   boolean loggedNoCounter = false;
   StringBuffer baseCounterName = null;
   int baseNameLength = 0;

   public CounterInterceptor() {
   }
   
   public void setContainer(Container container) {
      baseCounterName = new StringBuffer(container.getBeanClass().getName());
      baseNameLength = baseCounterName.length();
      this.container = container;
   }
   public Container getContainer() {
      return container;
   }

   public Object invokeHome(Invocation mi) throws Exception {
      long startTime=System.currentTimeMillis();
      try {
         return super.invokeHome(mi);
      } finally {
         if (getCounter() != null) {
            long endTime=System.currentTimeMillis();
            baseCounterName.append("Home.");
            baseCounterName.append(mi.getMethod().getName());
            counter.accumulate(baseCounterName.toString(), endTime-startTime);
            baseCounterName.setLength(baseNameLength);
         }
      }
   }

   public Object invoke(Invocation mi) throws Exception {
      long startTime=System.currentTimeMillis();
      try {
         return super.invoke(mi);
      } finally {
         if (getCounter() != null) {
            long endTime=System.currentTimeMillis();
            baseCounterName.append('.');
            baseCounterName.append(mi.getMethod().getName());
            counter.accumulate(baseCounterName.toString(), endTime-startTime);
            baseCounterName.setLength(baseNameLength);
         }
      }
   }

   public void create() throws java.lang.Exception {
      //get a reference to the CounterService from JNDI
      log.debug("CounterInterceptor initializing");
   }

   private CounterService getCounter() {
      if (counter == null) {
         try {
            InitialContext ctx = new InitialContext();
            counter = (CounterService)ctx.lookup(CounterService.JNDI_NAME);
         } catch (NamingException ne) {
            if (!loggedNoCounter) {
               log.warn("CounterInterceptor can't get counter service ", ne);
               loggedNoCounter = true;
            }
         }
      }
      return counter;
   }

   // Monitorable implementation ------------------------------------
   
   public void sample(Object s)
   {
      // Just here to because Monitorable request it but will be removed soon
   }
   
   public Map retrieveStatistic()
   {
      return null;
   }
   
   public void resetStatistic()
   {
   }
}
