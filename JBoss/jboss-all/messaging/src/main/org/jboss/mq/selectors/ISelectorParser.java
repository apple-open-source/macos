package org.jboss.mq.selectors;

import java.io.StringReader;
import java.util.HashSet;
import java.util.HashMap;

/** An interface describing a JMS selector expression parser.

@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
 */
public interface ISelectorParser
{
   public Object parse(String selector, HashMap identifierMap) throws Exception;
   public Object parse(String selector, HashMap identifierMap, boolean trace) throws Exception;
}
