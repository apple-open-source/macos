package org.jboss.test.perf.interfaces;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;
import javax.ejb.FinderException;

/**
 @author Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public interface EntityLocalHome extends EJBLocalHome
{
  EntityLocal create(int key, int value) 
    throws CreateException;

  EntityLocal findByPrimaryKey(EntityPK primaryKey) 
    throws FinderException;

  Collection findInRange(int min, int max) 
    throws FinderException;
}

