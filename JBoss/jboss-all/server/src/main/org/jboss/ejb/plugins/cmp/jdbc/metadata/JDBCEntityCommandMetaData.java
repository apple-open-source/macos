/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.util.Iterator;
import java.util.HashMap;
import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;

/**
 * This immutable class contains information about entity command
 *
 * @author <a href="mailto:loubyansky@ua.fm">Alex Loubyansky</a>
 * @version $Revision: 1.2.2.1 $
 */
public final class JDBCEntityCommandMetaData
{

   // Attributes -----------------------------------------------------

   /** The name (alias) of the command. */
   private final String commandName;

   /** The class of the command */
   private final Class commandClass;

   /** Command attributes */
   private final HashMap attributes = new HashMap();
  

   // Constructor ----------------------------------------------------

   /**
    * Constructs a JDBCEntityCommandMetaData reading the entity-command element
    * @param entity-command element
    */
   public JDBCEntityCommandMetaData( Element element )
      throws DeploymentException
   {
      // command name
      commandName = element.getAttribute( "name" );
      if( commandName.trim().length() < 1 )
      {
         throw new DeploymentException( "entity-command element must have "
            + " not empty name attribute" );
      }

      String commandClassStr = element.getAttribute( "class" );
      if(commandClassStr != null)
      {
         try
         {
            commandClass = Thread.currentThread().
               getContextClassLoader().loadClass( commandClassStr );
         } catch (ClassNotFoundException e) {
            throw new DeploymentException( "Could not load class: "
               + commandClassStr);
         }
      }
      else
      {
         commandClass = null;
      }

      // attributes
      for( Iterator iter = MetaData.getChildrenByTagName( element, "attribute" );
         iter.hasNext(); )
      {
         Element attrEl = (Element) iter.next();

         // attribute name
         String attrName = attrEl.getAttribute( "name" );
         if( attrName == null )
         {
            throw new DeploymentException( "entity-command " + commandName
               + " has an attribute with no name" );
         }

         // attribute value
         String attrValue = MetaData.getElementContent( attrEl );

         attributes.put( attrName, attrValue );
      }
   }

   /**
    * Constructs a JDBCEntityCommandMetaData from entity-command  xml element
    * and default values
    * @param entity-command element
    */
   public JDBCEntityCommandMetaData( Element element,
                                     JDBCEntityCommandMetaData defaultValues )
      throws DeploymentException
   {
      // command name
      commandName = defaultValues.getCommandName();

      String commandClassStr = element.getAttribute( "class" );
      if( (commandClassStr != null)
         && (commandClassStr.trim().length() > 0) )
      {
         try
         {
            commandClass = Thread.currentThread().
               getContextClassLoader().loadClass( commandClassStr );
         } catch (ClassNotFoundException e) {
            throw new DeploymentException( "Could not load class: "
               + commandClassStr);
         }
      }
      else
      {
         commandClass = defaultValues.getCommandClass();
      }

      // attributes
      attributes.putAll( defaultValues.attributes );
      for( Iterator iter = MetaData.getChildrenByTagName( element, "attribute" );
         iter.hasNext(); )
      {
         Element attrEl = (Element) iter.next();

         // attribute name
         String attrName = attrEl.getAttribute( "name" );
         if( attrName == null )
         {
            throw new DeploymentException( "entity-command " + commandName
               + " has an attribute with no name" );
         }

         // attribute value
         String attrValue = MetaData.getElementContent( attrEl );

         attributes.put( attrName, attrValue );
      }
   }

   // Public ----------------------------------------------------------

   /**
    * @return the name of the command
    */
   public String getCommandName() {
      return commandName;
   }

   /**
    * @return the class of the command
    */
   public Class getCommandClass() {
      return commandClass;
   }

   /**
    * @return value for the passed in parameter name
    */
   public String getAttribute( String name )
   {
      return (String) attributes.get( name );
   }

   // Object overrides --------------------------------------------------

   public String toString()
   {
      return new StringBuffer( "[commandName=" ).append( commandName ).
         append( ",commandClass=" ).append( commandClass ).
         append( ",attributes=" ).append( attributes.toString() ).
         append( "]" ).toString();
   }
}
