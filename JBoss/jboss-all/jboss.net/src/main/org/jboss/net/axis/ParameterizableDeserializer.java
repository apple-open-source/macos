/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ParameterizableDeserializer.java,v 1.2 2002/04/26 11:53:25 cgjung Exp $

package org.jboss.net.axis;

import javax.xml.rpc.encoding.Deserializer;
import java.util.Map;

/**
 * Extended deserializer that may be equipped with additional
 * options.
 * @author jung
 * @created 06.04.2002
 * @version $Revision: 1.2 $
 */

public interface ParameterizableDeserializer extends Deserializer {

	/** return the set of options */
	public Map getOptions();
	/** register a set of options */
	public void setOptions(Map options);
		
}