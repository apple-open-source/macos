package org.objectweb.jtests.providers.admin;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.ObjectName;
import javax.naming.*;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.mx.util.MBeanProxy;
import org.objectweb.jtests.jms.admin.Admin;

import javax.jms.Queue;
import javax.jms.Topic;
import java.util.*;
import java.io.*;
import java.net.InetAddress;

public class JBossMQAdmin implements Admin {
  
    private String name = "JBossMQ";
    InitialContext ictx = null;
    RMIAdaptor server;

    public JBossMQAdmin() {
	try {
	    Properties props = new Properties();
	    props.setProperty("java.naming.factory.initial", "org.jnp.interfaces.NamingContextFactory");
	    props.setProperty("java.naming.factory.url.pkgs", "org.jboss.naming:org.jnp.interfaces");
	    props.setProperty("java.naming.provider.url", "localhost");
	    ictx = new InitialContext (props);
	    
               String serverName = System.getProperty("jbosstest.server.name");
               if (serverName == null) {
                  serverName = InetAddress.getLocalHost().getHostName();
               }
               server = (RMIAdaptor)ictx.lookup("jmx:" + serverName + ":rmi");
               
	} catch (Exception e) {
	    e.printStackTrace();
	}
    }


    public String getName() {
	return name;
    }

    public InitialContext createInitialContext() throws NamingException {
	return ictx;
    }
  
    public void createQueueConnectionFactory(String name) {
       try {
          
          String mbeanClass = "org.jboss.naming.NamingAlias";
          ObjectName objn = new ObjectName("testsuite:service=NamingAlias,fromName="+name);
          server.createMBean(mbeanClass, objn);
          server.setAttribute(objn, new Attribute("ToName", "ConnectionFactory"));
          server.setAttribute(objn, new Attribute("FromName", name));
          server.invoke(objn, "create", new Object[]{}, new String[]{});
          server.invoke(objn, "start", new Object[]{},new String[]{});
       } catch (Exception e ) {
          e.printStackTrace();
       }
    }


    public void deleteQueueConnectionFactory(String name) {
       try {
          ObjectName objn = new ObjectName("testsuite:service=NamingAlias,fromName="+name);
          if( server.isRegistered(objn) ) {
             server.invoke(objn, "stop", new Object[]{}, new String[]{});
             server.invoke(objn, "destroy", new Object[]{}, new String[]{});
             server.unregisterMBean(objn);
          }
       } catch (Exception e ) {
          e.printStackTrace();
       }
    }
    
    public void createTopicConnectionFactory(String name) {
       createQueueConnectionFactory(name);
    }
    
    public void deleteTopicConnectionFactory(String name) {
       deleteQueueConnectionFactory(name);
    }
 
    public void createQueue(String name) {
       
       try {
        ObjectName objn = new ObjectName("jboss.mq:service=DestinationManager");
        server.invoke(objn, "createQueue", new Object[]{"testsuite-"+name,name}, new String[] {String.class.getName(), String.class.getName()});
       } catch (Exception e ) {
          e.printStackTrace();
       }
    }
  
    public void createTopic(String name) {
       try {
        ObjectName objn = new ObjectName("jboss.mq:service=DestinationManager");
        server.invoke(objn, "createTopic", new Object[]{"testsuite-"+name,name}, new String[] {String.class.getName(), String.class.getName()});
       } catch (Exception e ) {
          e.printStackTrace();
       }
    }

    public void deleteQueue(String name) {
       try {
        ObjectName objn = new ObjectName("jboss.mq:service=DestinationManager");
        server.invoke(objn, "destroyQueue", new Object[]{"testsuite-"+name}, new String[] {String.class.getName()});
       } catch (Exception e ) {
          e.printStackTrace();
       }
    }
 
    public void deleteTopic(String name) {
       try {
        ObjectName objn = new ObjectName("jboss.mq:service=DestinationManager");
        server.invoke(objn, "destroyTopic", new Object[]{"testsuite-"+name}, new String[] {String.class.getName()});
       } catch (Exception e ) {
          e.printStackTrace();
       }
    }

}
