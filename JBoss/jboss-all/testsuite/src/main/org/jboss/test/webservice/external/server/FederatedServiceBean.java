/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: FederatedServiceBean.java,v 1.1.2.2 2003/09/30 21:42:06 starksm Exp $

package org.jboss.test.webservice.external.server;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.jboss.test.webservice.external.google.GoogleSearchPort;
import org.jboss.test.webservice.external.google.GoogleSearchService;
import org.jboss.test.webservice.external.google.GoogleSearchResult;

import org.jboss.test.webservice.external.babelfish.BabelFishPortType;
import org.jboss.test.webservice.external.babelfish.BabelFishService;
import org.jboss.logging.Logger;

import javax.naming.InitialContext;

import javax.xml.rpc.Service;

/**
 * implementation of a federated ejb service making
 * use of external web services bound in the JNDI tree.
 * @version 	1.0
 * @author cgjung
 */

public class FederatedServiceBean implements SessionBean
{
   private static Logger log = Logger.getLogger(FederatedServiceBean.class);

   /** 
    * first accesses google to produce a set of
    * search results and then connects to babelfish
    * to translate the content of the title of 
    * one of them
    */

   public String findAndTranslate(String searchTerm) throws Exception
   {
      // look into JNDI
      InitialContext initContext = new InitialContext();
		
      // find external references there
      GoogleSearchService googleService = (GoogleSearchService)
         initContext.lookup("Google");
      GoogleSearchPort google = googleService.
         getGoogleSearchPort();

      String licenseKey =
         System.getProperty("google.license", "Wr5iTf5QFHJKmmnJn+61lt9jaMuWMKCj");

      GoogleSearchResult searchResult =
         google.doGoogleSearch(licenseKey, searchTerm, 0, 10, true, "", false, "", "latin1", "latin1");
      log.debug("Query for: '"+searchTerm+"' returned: "+searchResult);

      BabelFishService babelFishService = (BabelFishService)
         initContext.lookup("BabelFish");

      BabelFishPortType babelFish = babelFishService.getBabelFishPort();
				
      // and call them
      String translationResult = babelFish.babelFish("en_de", searchTerm);

      return translationResult;
   }

   //
   // the usual mumbo-jumbo
   //
	
   public void ejbCreate()
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbRemove()
   {
   }

   public void setSessionContext(SessionContext ctx)
   {
   }

}
