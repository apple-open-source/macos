package org.jboss.verifier.factory;

/*
 * Class org.jboss.verifier.factory.DefaultEventFactory
 * Copyright (C) 2000  Juha Lindfors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This package and its source code is available at www.jboss.org
 * $Id: DefaultEventFactory.java,v 1.6 2002/06/04 15:18:10 lqd Exp $
 */

// standard imports
import java.util.MissingResourceException;
import java.util.Properties;
import java.util.Set;
import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import java.util.Enumeration;

import java.io.InputStream;
import java.io.IOException;

// non-standard class dependencies
import org.jboss.verifier.Section;

import org.jboss.verifier.event.VerificationEvent;
import org.jboss.verifier.event.VerificationEventGenerator;

/**
 *
 * @author  Juha Lindfors   (jplindfo@helsinki.fi)
 * @version $Revision: 1.6 $
 * @since   JDK 1.3
 */
public class DefaultEventFactory
   implements VerificationEventFactory
{
   public final static String DEFAULT_MESSAGE_BUNDLE =
      "/org/jboss/verifier/DefaultMessages.properties";

   private Map msgTable = null;
   private String msgBundle;

   /**
    * Default constructor using the DEFAULT_MESSAGE_BUNDLE message
    * file.
    *
    * @deprecated Use the other constructor with a specific Message
    *             Bundle for your own verification logic!
    */
   public DefaultEventFactory()
   {
      this.msgBundle = DEFAULT_MESSAGE_BUNDLE;
      msgTable = loadErrorMessages();
   }

   /**
    * Create a DefaultEventFactory using the specified message bundle
    * for creating the Specification Violation Events
    */
   public DefaultEventFactory( String msgBundle )
   {
      this.msgBundle = "/org/jboss/verifier/" + msgBundle;
      msgTable = loadErrorMessages();
   }

   public VerificationEvent createSpecViolationEvent(
      VerificationEventGenerator source, Section section)
   {
      VerificationEvent event = new VerificationEvent(source);

      event.setState(VerificationEvent.WARNING);
      event.setSection(section);
      event.setMessage((String)msgTable.get(section.getSection()));

      return event;
   }

   public VerificationEvent createBeanVerifiedEvent(
      VerificationEventGenerator source)
   {
      VerificationEvent event = new VerificationEvent(source);

      event.setState(VerificationEvent.OK);
      event.setMessage("Verified.");

      return event;
   }

   public String getMessageBundle() {
      return msgBundle;
   }

/*
 *****************************************************************************
 *
 *  PRIVATE INSTANCE METHODS
 *
 *****************************************************************************
 */

   /*
    * loads messages from a property file
    */
   private Map loadErrorMessages()
   {
      try
      {
         InputStream in = getClass().getResourceAsStream( msgBundle );
         Properties  props = new Properties();
         props.load(in);

        return props;
      }
      catch (IOException e)
      {
         throw new MissingResourceException( "I/O failure: " +
            e.getMessage(), msgBundle, "" );
      }
      catch (NullPointerException e)
      {
         throw new MissingResourceException( "Resource not found.",
            msgBundle, "" );
      }
   }

}
/*
vim:ts=3:sw=3:et
*/
