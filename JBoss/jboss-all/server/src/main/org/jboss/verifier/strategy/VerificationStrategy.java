package org.jboss.verifier.strategy;

/*
 * Class org.jboss.verifier.strategy.VerificationStrategy
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
 * $Id: VerificationStrategy.java,v 1.8 2002/05/31 13:02:14 lqd Exp $
 */


// standard imports
import java.util.Iterator;

// non-standard class dependencies
import org.gjt.lindfors.pattern.Strategy;

import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.MessageDrivenMetaData;
import org.jboss.metadata.SessionMetaData;

/**
 * @author  <a href="mailto:juha.lindfors@jboss.org">Juha Lindfors</a>
 * @version $Revision: 1.8 $
 * @since   JDK 1.3
 */
public interface VerificationStrategy
   extends Strategy
{
   /**
    * Does the entity check
    */
   abstract void checkEntity( EntityMetaData entity );

   /**
    * Checks the session
    */
   abstract void checkSession( SessionMetaData session );

   /**
    * Checks the message driven beans (EJB 2.0 only).
    */
   abstract void checkMessageBean( MessageDrivenMetaData message );
}
/*
vim:ts=3:sw=3:et
*/
