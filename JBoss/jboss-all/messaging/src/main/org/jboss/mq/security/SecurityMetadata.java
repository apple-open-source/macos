/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.security;

import java.util.Set;
import java.util.HashSet;
import java.util.HashMap;
import java.io.StringReader;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Attr;
import org.w3c.dom.NodeList;

import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

import org.jboss.security.SimplePrincipal;
import org.jboss.logging.Logger;
/**
 * SecurityMetadata.java
 *
 *
 * Created: Tue Feb 26 15:02:29 2002
 *
 * @author Peter
 * @version
 */

public class SecurityMetadata  {
   static Role DEFAULT_ROLE = new Role("guest", true, true, true);

   static class Role {
      String name;
      boolean read= false;
      boolean write = false;
      boolean create = false;
      public Role(String name, boolean read, boolean write, boolean create) {
         this.name = name;
         this.read = read;
         this.write = write;
         this.create = create;
      }
      public String toString() {
         return "Role {name="+name+";read="+read+";write="+write+";create="+create+"}";
      }

   }


   HashMap roles = new HashMap();
   HashSet read = new HashSet();
   HashSet write = new HashSet();
   HashSet create = new HashSet();
   static Logger log = Logger.getLogger(SecurityMetadata.class);
   
   public SecurityMetadata() {
      addRole(DEFAULT_ROLE);
   }
   /**
    * Create with given xml @see configure.
    *
    * If the configure script is null, a default role named guest will be
    * created with read and write access, but no create access.
    */
   public SecurityMetadata(String conf)throws Exception {
      configure(conf);
   }
   public SecurityMetadata(Element conf)throws Exception {
      configure(conf);
   }
   /**
    * Configure with an xml string.
    *
    * The format of the string is:
    * <security>
    *  <role name="nameOfRole" read="true" write="true" create="false"/>
    * </security>
    *
    * There may be one or more role elements.
    */
   public void configure(String conf) throws Exception {
      Element sec = null;
      if (conf != null) {
         DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
         DocumentBuilder parser = factory.newDocumentBuilder();
         Document doc = parser.parse(new InputSource(new StringReader(conf)));
         sec = doc.getDocumentElement();
         
      }
      configure(sec);
   }
      
   public void configure(Element sec) throws Exception {
   
      if (sec == null) {
         addRole(DEFAULT_ROLE);
      }else {

         if (!sec.getTagName().equals("security"))
            throw new SAXException("Configuration document not valid: root element must be security, not " + sec.getTagName());  
         
         // Parse 
         NodeList list = sec.getElementsByTagName("role");
         int l = list.getLength();
         for(int i = 0; i<l;i++) {
            Element role = (Element)list.item(i);
            Attr na = role.getAttributeNode("name");
            if (na == null)
               throw new SAXException("There must exist a name attribute of role");
            String n = na.getValue();
            boolean r = role.getAttributeNode("read") != null ? Boolean.valueOf( role.getAttributeNode("read").getValue() ).booleanValue() : false;
            boolean w = role.getAttributeNode("write") != null ? Boolean.valueOf( role.getAttributeNode("write").getValue() ).booleanValue() : false;
            boolean c = role.getAttributeNode("create") != null ? Boolean.valueOf( role.getAttributeNode("create").getValue() ).booleanValue() : false;
            addRole(n,r,w,c);
            
         }
      }
   }

   public void addRole(String name,  boolean read, boolean write, boolean create) {
      Role r = new Role(name,read,write,create);
      addRole(r);
   }
   
   public void addRole(Role r) {
      if (log.isTraceEnabled())
         log.trace("Adding role: " + r.toString());

      roles.put(r.name,r);
      SimplePrincipal p = new SimplePrincipal(r.name);
      if(r.read == true)
         read.add(p);
      if(r.write == true)
         write.add(p);
      if (r.create == true)
         create.add(p);
   }
   
   public Set getReadPrincipals() {
      return read;
   }
   
   public Set getWritePrincipals() {
      return write;
   }

   public Set getCreatePrincipals() {
      return create;
   }
} // SecurityMetadata
