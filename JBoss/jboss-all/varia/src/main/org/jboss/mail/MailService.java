/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.mail;

import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.util.Properties;

import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;

import javax.naming.InitialContext;
import javax.naming.Reference;
import javax.naming.StringRefAddr;
import javax.naming.Name;
import javax.naming.Context;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;

import javax.mail.Session;
import javax.mail.PasswordAuthentication;
import javax.mail.Authenticator;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.naming.NonSerializableFactory;

/**
 * MBean that gives support for JavaMail. Object of class javax.mail.Session will be bound
 * in JNDI under java:/ namespace with the name provided with method {@link #setJNDIName}.
 *
 * @jmx:mbean name="jboss:type=Service,service=Mail"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.9.2.3 $</tt>
 * @author  <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MailService
      extends ServiceMBeanSupport
      implements MailServiceMBean
{
   public static final String JNDI_NAME = "java:/Mail";

   private String user;
   private String password;
   private String jndiName = JNDI_NAME;
   private Element config;

   /** Object Name of the JSR-77 representant of this service */
   ObjectName mMail;
   
   /** save properties here */
   Properties ourProps = null;

   /**
    * User id used to connect to a mail server
    *
    * @see #setPassword
    *
    * @jmx:managed-attribute
    */
   public void setUser(final String user)
   {
      this.user = user;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getUser()
   {
      return user;
   }

   /**
    * Password used to connect to a mail server
    *
    * @see #setUser
    *
    * @jmx:managed-attribute
    */
   public void setPassword(final String password)
   {
      this.password = password;
   }

   /**
    * Password is write only.
    */
   protected String getPassword()
   {
      return password;
   }

   /**
    * Configuration for the mail service.
    *
    * @jmx:managed-attribute
    */
   public Element getConfiguration()
   {
      return config;
   }

   /**
    * Configuration for the mail service.
    *
    * @jmx:managed-attribute
    */
   public void setConfiguration(final Element element)
   {
      config = element;
   }

   /**
    * The JNDI name under the java:/ namespace to which javax.mail.Session
    * objects are bound.
    *
    * @jmx:managed-attribute
    */
   public void setJNDIName(final String name)
   {
      if (!name.startsWith("java:/"))
      {
         jndiName = "java:/" + name;
      }
      else
      {
         jndiName = name;
      }
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJNDIName()
   {
      return jndiName;
   }

	/**
	 * @jmx:managed-attribute
	 */
	public String getStoreProtocol()
	{
		if (ourProps!=null)
			return ourProps.getProperty("mail.store.protocol");
		else
			return null;   	
	}
	/**
	 * @jmx:managed-attribute
	 */
	public String getTransportProtocol()
	{
		if (ourProps!=null)
			return ourProps.getProperty("mail.transport.protocol");
		else
			return null;   	
	}
	/**
	 * @jmx:managed-attribute
	 */
	public String getDefaultSender()
	{
		if (ourProps!=null)
			return ourProps.getProperty("mail.from");
		else
			return null;   	
	}


	/**
	 * @jmx:managed-attribute
	 */
   public String getSMTPServerHost()
   {
		if (ourProps!=null)
			return ourProps.getProperty("mail.smtp.host");
		else
			return null;   	
   }

	/**
	 * @jmx:managed-attribute
	 */
   public String getPOP3ServerHost()
   {
   	if (ourProps!=null)
   		return ourProps.getProperty("mail.pop3.host");
		else
			return null;
   }

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
         throws MalformedObjectNameException
   {
      return name == null ? OBJECT_NAME : name;
   }

   protected void startService() throws Exception
   {
      // Setup password authentication
      final PasswordAuthentication pa = new PasswordAuthentication(getUser(), getPassword());
      Authenticator a = new Authenticator()
      {
         protected PasswordAuthentication getPasswordAuthentication()
         {
            return pa;
         }
      };

      Properties props = getProperties();

      // Finally create a mail session
      Session session = Session.getInstance(props, a);
      bind(session);
      
      // now make the properties available
      ourProps= props;
   }

   protected Properties getProperties() throws Exception
   {
      boolean debug = log.isDebugEnabled();

      Properties props = new Properties();
      if (config == null)
      {
         log.warn("No configuration specified; using empty properties map");
         return props;
      }

      NodeList list = config.getElementsByTagName("property");
      int len = list.getLength();

      for (int i = 0; i < len; i++)
      {
         Node node = list.item(i);

         switch (node.getNodeType())
         {
            case Node.ELEMENT_NODE:
               Element child = (Element) node;
               String name, value;

               // get the name
               if (child.hasAttribute("name"))
               {
                  name = child.getAttribute("name");
               }
               else
               {
                  log.warn("Ignoring invalid element; missing 'name' attribute: " + child);
                  break;
               }

               // get the value
               if (child.hasAttribute("value"))
               {
                  value = child.getAttribute("value");
               }
               else
               {
                  log.warn("Ignoring invalid element; missing 'value' attribute: " + child);
                  break;
               }

               if (debug)
               {
                  log.debug("setting property " + name + "=" + value);
               }
               props.setProperty(name, value);
               break;

            case Node.COMMENT_NODE:
               // ignore
               break;

            default:
               log.debug("ignoring unsupported node type: " + node);
               break;
         }
      }

      if (debug)
      {
         log.debug("Using properties: " + props);
      }

      return props;
   }

   protected void stopService() throws Exception
   {
      unbind();
   }

   private void bind(Session session) throws NamingException
   {
      String bindName = getJNDIName();

      // Ah ! Session isn't serializable, so we use a helper class
      NonSerializableFactory.bind(bindName, session);

      Context ctx = new InitialContext();
      try
      {
         Name n = ctx.getNameParser("").parse(bindName);
         while (n.size() > 1)
         {
            String ctxName = n.get(0);
            try
            {
               ctx = (Context) ctx.lookup(ctxName);
            }
            catch (NameNotFoundException e)
            {
               ctx = ctx.createSubcontext(ctxName);
            }
            n = n.getSuffix(1);
         }


         // The helper class NonSerializableFactory uses address type nns, we go on to
         // use the helper class to bind the javax.mail.Session object in JNDI

         StringRefAddr addr = new StringRefAddr("nns", bindName);
         Reference ref = new Reference(Session.class.getName(),
               addr,
               NonSerializableFactory.class.getName(),
               null);
         ctx.bind(n.get(0), ref);
      }
      finally
      {
         ctx.close();
      }

      log.info("Mail Service bound to " + bindName);
   }

   private void unbind() throws NamingException
   {
      String bindName = getJNDIName();

      if (bindName != null)
      {
         InitialContext ctx = new InitialContext();
         try
         {
            ctx.unbind(bindName);
         }
         finally
         {
            ctx.close();
         }

         NonSerializableFactory.unbind(bindName);
         log.info("Mail service '" + getJNDIName() + "' removed from JNDI");
      }
   }
}
