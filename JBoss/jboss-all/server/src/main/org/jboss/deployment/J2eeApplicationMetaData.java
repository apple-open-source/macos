/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.deployment;



import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;

/**
 *	<description> 
 *      
 *	@see <related>
 *	@author <firstname> <lastname> (<email>)
 *	@version $Revision: 1.5 $
 */
public class J2eeApplicationMetaData
   extends MetaData
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
	String displayName;
   String description;
   String smallIcon;
   String largeIcon;
	
   final Collection modules = new HashSet();
	
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   public J2eeApplicationMetaData (Element rootElement) throws DeploymentException
   {
      importXml (rootElement, false);
   }
	
   // Public --------------------------------------------------------

   public String getDisplayName ()
   {
      return displayName;
   }
   
   public String getDescription ()
   {
      return description;
   }
   
   public String getSmallIcon ()
   {
      return smallIcon;
   }
   
   public String getLargeIcon ()
   {
      return largeIcon;
   }
   
   public Iterator getModules ()
   {
      return modules.iterator ();
   }
   



    public void importXml (Element element, boolean jbossSpecific) throws DeploymentException
    {
       String rootTag = element.getOwnerDocument().getDocumentElement().getTagName();
       
       if ((rootTag.equals("application") && !jbossSpecific) ||
           (rootTag.equals("jboss-app") && jbossSpecific)) 
       {
			
          // get some general info
          if (!jbossSpecific) 
          {
             displayName = getElementContent (getUniqueChild (element, "display-name"));
          } // end of if ()
          
          Element e = getOptionalChild (element, "description");
          description = e != null ? getElementContent (e) : "";

          e = getOptionalChild (element, "icon");
          if (e != null)
          {
             Element e2 = getOptionalChild (element, "small-icon");
             smallIcon = e2 != null ? getElementContent (e2) : "";
             
             e2 = getOptionalChild (element, "large-icon");
             largeIcon = e2 != null ? getElementContent (e2) : "";
          }
          else
          {
             smallIcon = "";
             largeIcon = "";
          }
			
          // extract modules...
          for (Iterator it = getChildrenByTagName (element, "module"); it.hasNext (); )
          {
             modules.add (new J2eeModuleMetaData((Element)it.next(), jbossSpecific));
          }
       }		
       else 
       {
          throw new DeploymentException("Unrecognized root tag in EAR deployment descriptor: "+ element);
       }
    }
   

    
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
