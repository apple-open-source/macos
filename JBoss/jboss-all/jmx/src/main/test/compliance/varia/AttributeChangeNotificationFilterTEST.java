/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.varia;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import java.util.Vector;

import junit.framework.TestCase;

import javax.management.*;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 */
public class AttributeChangeNotificationFilterTEST
  extends TestCase
{

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public AttributeChangeNotificationFilterTEST(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  public void testGetEnabledAttributes()
  {
      AttributeChangeNotificationFilter filter = new AttributeChangeNotificationFilter();
      
      assertTrue(filter.getEnabledAttributes().size() == 0);
      
      filter.enableAttribute("foo");
      filter.enableAttribute("bar");
      
      assertTrue(filter.getEnabledAttributes().size() == 2);
  }

  public void testDisableAttribute()
  {
     AttributeChangeNotificationFilter filter = new AttributeChangeNotificationFilter();
     
     filter.enableAttribute("foo");
     filter.enableAttribute("bar");
     
     assertTrue(filter.getEnabledAttributes().size() == 2);
     
     filter.disableAttribute("foo");
     
     assertTrue(filter.getEnabledAttributes().size() == 1);
     assertTrue(filter.getEnabledAttributes().get(0).equals("bar"));
     
     filter.disableAllAttributes();
     
     assertTrue(filter.getEnabledAttributes().size() == 0);
     
  }

}
