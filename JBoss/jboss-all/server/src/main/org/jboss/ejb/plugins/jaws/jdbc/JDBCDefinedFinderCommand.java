/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.jdbc;

import java.util.ArrayList;
import java.util.StringTokenizer;
import java.util.Set;
import java.util.HashSet;

import java.sql.PreparedStatement;

import org.jboss.ejb.plugins.jaws.metadata.FinderMetaData;
import org.jboss.ejb.plugins.jaws.metadata.TypeMappingMetaData;

import org.jboss.logging.Logger;

/**
 * JAWSPersistenceManager JDBCDefinedFinderCommand
 *
 * @see <related>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:shevlandj@kpi.com.au">Joe Shevland</a>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @author <a href="mailto:menonv@cpw.co.uk">Vinay Menon</a>
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson)</a>
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @author <a href="mailto:lennart.petersson@benefit.se">Lennart Petersson</a>
 * @version $Revision: 1.21 $
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010621 Bill Burke:</b>
 *   <ul>
 *   <li>exposed parameterArray through get method.
 *   </li>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   </ul>
 *
 */
public class JDBCDefinedFinderCommand extends JDBCFinderCommand
{
   // Attributes ----------------------------------------------------

   private Logger log = Logger.getLogger(JDBCDefinedFinderCommand.class);
   private int[] parameterArray;
   private TypeMappingMetaData typeMapping;

   private String fromClause = "";
   private String whereClause = "";
   private String orderClause = "";

   // Constructors --------------------------------------------------

   public JDBCDefinedFinderCommand(JDBCCommandFactory factory, FinderMetaData f)
   {
      super(factory, f);

      typeMapping = jawsEntity.getJawsApplication().getTypeMapping();

      // Replace placeholders with ?, but only if query is defined
      String query = "";
      ArrayList parameters = new ArrayList();
      if (f.getQuery() != null)  {
         StringTokenizer finderQuery = new StringTokenizer(f.getQuery(),"{}", true);

         while (finderQuery.hasMoreTokens())
         {
            String t = finderQuery.nextToken();
            if (t.equals("{"))
            {
               query += "?";
               String idx = finderQuery.nextToken(); // Remove number
               parameters.add(new Integer(idx));
               finderQuery.nextToken(); // Remove }
            } else
               query += t;
         }
      }

      // Copy index numbers to parameterArray
      parameterArray = new int[parameters.size()];
      for (int i = 0; i < parameterArray.length; i++)
         parameterArray[i] = ((Integer)parameters.get(i)).intValue();

      // Since the fields in order clause also will form the select clause together with
      // the pk field list, we have to clean the order clause from ASC/DESC's and fields
      // that already are within the pk list
      // Note that extraOrderColumns will start with a ','
      String extraOrderColumns = getExtraOrderColumns(f);

      String lcQuery = query.toLowerCase();
      // build from clause, including any joins specified by the deployer/assembler
      // In case of join query:
      // order must explicitly identify tablename.field to order on
      // query must start with "INNER JOIN <table to join with> WHERE
      // <regular query with fully identified fields>"
      if (lcQuery.startsWith(",") || lcQuery.startsWith("inner join")) {
         //this is the case of a 'where' that is build to actually join tables:
         //  ,table2 as foo where foo.col1 = entitytable.col2 AND entitytable.filter = {1}
         // or
         //  inner join table2 on table2.col1 = entitytable.col2 AND entitytable.filter = {1}
         String tableList = null;
         int whereStart = lcQuery.indexOf("where");
         if (whereStart == -1) {
            //log this at debug in case someone has made a mistake, but assume that
            // they mean a findAll.
            if (log.isDebugEnabled()) log.debug("Strange query for finder "+f.getName()+
               ". Includes join, but no 'where' clause. Is this a findAll?");
            tableList = query;
            whereClause = "";
         } else {
            tableList = query.substring(0, whereStart);
            whereClause = query.substring(whereStart);
         }
         fromClause = "FROM "+jawsEntity.getTableName()+tableList;
      } else {
         fromClause = "FROM "+jawsEntity.getTableName();
         if (lcQuery.startsWith("where"))
            whereClause = query;
         else
            whereClause = "where "+query;
      }


      StringBuffer sqlBuffer = new StringBuffer();
      sqlBuffer.append("SELECT ");
      //where clauseString primaryKeyList = getPkColumnList();
      String tableName = jawsEntity.getTableName();
      StringTokenizer stok = new StringTokenizer(getPkColumnList(),",");

      while(stok.hasMoreTokens()){
        sqlBuffer.append(tableName);
        sqlBuffer.append(".");
        sqlBuffer.append(stok.nextElement().toString());
        sqlBuffer.append(",");
      }
      // ditch the last ',' at the end...
      sqlBuffer.setLength(sqlBuffer.length()-1);
      // because it's already on the front of extraOrderColumns
      sqlBuffer.append(extraOrderColumns);
      sqlBuffer.append(' ');
      sqlBuffer.append(fromClause);
      sqlBuffer.append(' ');
      sqlBuffer.append(whereClause);

      if (f.getOrder() != null && !f.getOrder().equals(""))
      {
         orderClause = " ORDER BY "+f.getOrder();
         sqlBuffer.append(orderClause);
      }
      setSQL(sqlBuffer.toString());
   }

   public String getWhereClause() {
      return whereClause;
   }

   public String getFromClause() {
      return fromClause;
   }
   public String getOrderByClause() {
      return orderClause;
   }

   public int[] getParameterArray()
   {
      return parameterArray;
   }

   /** helper method to clean the order clause into a list of table.field
    *  entries. This is used only to clean up the algorythm in the ctor.
    *  @return String array containing order fields stripped of 'ASC' or 'DESC'
    *  modifiers.
    */
   protected String[] cleanOrderClause(String rawOrder) {
     //Split it into tokens. These tokens might contain ASC/DESC that we have to get rid of
     StringTokenizer orderTokens = new StringTokenizer(rawOrder, ",");
     String orderToken;
     String[] checkedOrderTokens = new String[orderTokens.countTokens()];
     int ix = 0;
     while(orderTokens.hasMoreTokens())
     {
       orderToken = orderTokens.nextToken().trim();
       //Get rid of ASC's
       int i = orderToken.toUpperCase().indexOf(" ASC");
       if(i!=-1)
         checkedOrderTokens[ix] = orderToken.substring(0, i).trim();
       else
       {
         //Get rid of DESC's
         i = orderToken.toUpperCase().indexOf(" DESC");
         if(i!=-1)
           checkedOrderTokens[ix] = orderToken.substring(0, i).trim();
         else
         {
           //No ASC/DESC - just use it as it is
           checkedOrderTokens[ix] = new String(orderToken).trim();
         }
       }
       ix++;
     }
     return checkedOrderTokens;
   }

   /** A helper method that 'folds' any columns specified in an order clause
    *  into the primary key fields so that they can be included in the select
    *  list <b>after</b> all primary key fields.
    */
   private String getExtraOrderColumns(FinderMetaData f) {
      String strippedOrder = "";
      if(f.getOrder()!=null && f.getOrder()!="")
      {
        String[] checkedOrderTokens = cleanOrderClause(f.getOrder());

        //Next step is to make up a Set of all pk tokens
        StringTokenizer pkTokens = new StringTokenizer(getPkColumnList(), ",");
        Set setOfPkTokens = new HashSet(pkTokens.countTokens());
        while(pkTokens.hasMoreTokens())
        {
          setOfPkTokens.add(pkTokens.nextToken().trim().toLowerCase());
        }

        //Now is the time to check for duplicates between pk and order tokens
        int i = 0;
        while(i < checkedOrderTokens.length)
        {
          //If duplicate token, null it away
          if(setOfPkTokens.contains(checkedOrderTokens[i].toLowerCase()))
          {
            checkedOrderTokens[i]=null;
          }
          i++;
        }

        //Ok, build a new order string that we can use later on
        StringBuffer orderTokensToUse = new StringBuffer("");
        i = 0;
        while(i < checkedOrderTokens.length)
        {
          if(checkedOrderTokens[i]!=null)
          {
            orderTokensToUse.append(", ");
            orderTokensToUse.append(checkedOrderTokens[i]);
          }
          i++;
        }
        // Note that orderTokensToUse will always start with ", " if there is any order tokens
        strippedOrder = orderTokensToUse.toString();
      }
      return strippedOrder;
   }
   // JDBCFinderCommand overrides ------------------------------------

   protected void setParameters(PreparedStatement stmt, Object argOrArgs)
      throws Exception
   {
      Object[] args = (Object[])argOrArgs;

      for (int i = 0; i < parameterArray.length; i++)
      {
          Object arg = args[parameterArray[i]];
          int jdbcType;

          if(arg!=null)
          {
             jdbcType = typeMapping.getJdbcTypeForJavaType(arg.getClass());
          }
          else
          {
             jdbcType = java.sql.Types.NULL;
          }

          setParameter(stmt,i+1,jdbcType,arg);
      }
   }
}
