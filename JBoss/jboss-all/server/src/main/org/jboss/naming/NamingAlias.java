/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.naming;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.LinkRef;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;

import org.jboss.system.ServiceMBean;
import org.jboss.system.ServiceMBeanSupport;

/**
 * A simple utility mbean that allows one to create an alias in
 * the form of a LinkRef from one JNDI name to another.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>.
 * @version $Revision: 1.6 $
 */
public class NamingAlias
   extends ServiceMBeanSupport
   implements NamingAliasMBean
{
   private String fromName;
   private String toName;

   public NamingAlias()
   {
      this(null, null);
   }
   
   public NamingAlias(final String fromName, final String toName)
   {
      this.fromName = fromName;
      this.toName = toName;
   }
   
   /**
    * Get the from name of the alias. This is the location where the
    * LinkRef is bound under JNDI.
    *
    * @jmx:managed-attribute
    * 
    * @return the location of the LinkRef
    */
   public String getFromName()
   {
      return fromName;
   }

   /**
    * Set the from name of the alias. This is the location where the
    * LinkRef is bound under JNDI.
    *
    * @jmx:managed-attribute
    * 
    * @param name, the location where the LinkRef will be bound
    */
   public void setFromName(String name) throws NamingException
   {
      removeLinkRef(fromName);
      this.fromName = name;
      createLinkRef();        
   }

   /**
    * Get the to name of the alias. This is the target name to
    * which the LinkRef refers. The name is a URL, or a name to be resolved
    * relative to the initial context, or if the first character of the name
    * is ".", the name is relative to the context in which the link is bound.
    *
    * @jmx:managed-attribute
    * 
    * @return the target JNDI name of the alias.
    */
   public String getToName()
   {
      return toName;
   }

   /**
    * Set the to name of the alias. This is the target name to
    * which the LinkRef refers. The name is a URL, or a name to be resolved
    * relative to the initial context, or if the first character of the name
    * is ".", the name is relative to the context in which the link is bound.
    *
    * @jmx:managed-attribute
    * 
    * @param name, the target JNDI name of the alias.
    */
   public void setToName(String name) throws NamingException
   {
      this.toName = name;
      
      createLinkRef();
   }
   
   protected void startService() throws Exception
   {
      if( fromName == null )
         throw new IllegalStateException("fromName is null");
      if( toName == null )
         throw new IllegalStateException("toName is null");
      createLinkRef();
   }
   
   protected void stopService() throws Exception
   {
      removeLinkRef(fromName);
   }
   
   private void createLinkRef() throws NamingException
   {
      if( super.getState() == ServiceMBean.STARTING || super.getState() == ServiceMBean.STARTED )
      {
         InitialContext ctx = new InitialContext();
         LinkRef link = new LinkRef(toName);
         Context fromCtx = ctx;
         Name name = ctx.getNameParser("").parse(fromName);
         String atom = name.get(name.size()-1);
         for(int n = 0; n < name.size()-1; n ++)
         {
            String comp = name.get(n);
            try
            {
               fromCtx = (Context) fromCtx.lookup(comp);
            }
            catch(NameNotFoundException e)
            {
               fromCtx = fromCtx.createSubcontext(comp);
            }
         }

         log.debug("atom: " + atom);
         log.debug("link: " + link);
         
         fromCtx.rebind(atom, link);

         log.info("Bound link " + fromName + " to " + toName);
      }
   }
   
   /**
    * Unbind the name value if we are in the STARTED state.
    */
   private void removeLinkRef(String name) throws NamingException
   {
      if( super.getState() == ServiceMBean.STARTED )
      {
         InitialContext ctx = new InitialContext();
         ctx.unbind(name);
      }
   }
}
