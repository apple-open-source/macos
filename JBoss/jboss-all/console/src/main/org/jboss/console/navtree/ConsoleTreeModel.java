/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.navtree;



import org.jboss.console.manager.interfaces.TreeInfo;

/**
 * TreeModel used to represent management information
 *
 * @see org.jboss.console.navtree.AdminTreeBrowser
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>17 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class ConsoleTreeModel implements javax.swing.tree.TreeModel
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected TreeInfo tree = null;
   protected RootWrapper root = null;
   protected java.util.Vector treeModelListeners = new java.util.Vector();
   //protected InitialContext ctx = null;
   //protected String pluginMgrJmxName = null;
   protected TreeContext context = null;
   protected TreeReopenerMemory reopenerMemory = null;
   
   protected javax.management.ObjectName targetPM = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public ConsoleTreeModel (TreeContext context) throws Exception
   {      
      //this.pluginMgrJmxName = context.getServiceJmxName();
      this.targetPM = new javax.management.ObjectName (context.getServiceJmxName());
      //this.ctx = new InitialContext (jndiProps);      
      this.context = context;
      //this.reopenerMemory = reopenerMemory;
      
      this.tree = loadTree ();      
      this.root = new RootWrapper (this.tree);
   }
   
   public boolean refreshTree (boolean force) throws Exception
   {
      TreeInfo tmpTree = null;
      
      if (!force && this.tree != null)
      {
         // first check if that is necessary
         //
         tmpTree = conditionalLoadTree (this.tree.getTreeVersion());  
      }
      else
      {
         tmpTree = loadTree ();
      }
      
      if (tmpTree != null)
      {
         RootWrapper oldRoot = this.root;                  
         
         this.tree = tmpTree;
         this.root = new RootWrapper (this.tree);
         
         fireTreeStructureChanged (oldRoot);
         return true;
      }
      else
      {
         return false;
      }
   }
   
   public TreeInfo conditionalLoadTree (long version) throws Exception
   {     
      return (TreeInfo)context.getRemoteMBeanInvoker ().invoke (targetPM, "getUpdateTreeForProfile", 
            new Object[] {"WEB", new Long(version)}, 
            new String[] {"java.lang.String", "long"});
      //return getPM ().getUpdateTreeForProfile ("WEB", version);            
   }
   
   public TreeInfo loadTree () throws Exception
   {      
      return (TreeInfo)context.getRemoteMBeanInvoker ().invoke (targetPM, "getTreeForProfile", 
            new Object[] {"WEB"}, 
            new String[] {"java.lang.String"});
      //return getPM ().getTreeForProfile ("WEB");
   }
   
   /** Adds a listener for the <code>TreeModelEvent</code>
    * posted after the tree changes.
    *
    * @param   l       the listener to add
    * @see     #removeTreeModelListener
    *
    */
   public void addTreeModelListener (javax.swing.event.TreeModelListener l)
   {
      treeModelListeners.addElement(l);
   }
   
   /** Returns the child of <code>parent</code> at index <code>index</code>
    * in the parent's
    * child array.  <code>parent</code> must be a node previously obtained
    * from this data source. This should not return <code>null</code>
    * if <code>index</code>
    * is a valid index for <code>parent</code> (that is <code>index >= 0 &&
    * index < getChildCount(parent</code>)).
    *
    * @param   parent  a node in the tree, obtained from this data source
    * @return  the child of <code>parent</code> at index <code>index</code>
    *
    */
   public Object getChild (Object parent, int index)
   {
      NodeWrapper n = (NodeWrapper)parent;
      return n.getChild (index);
   }
   
   /** Returns the number of children of <code>parent</code>.
    * Returns 0 if the node
    * is a leaf or if it has no children.  <code>parent</code> must be a node
    * previously obtained from this data source.
    *
    * @param   parent  a node in the tree, obtained from this data source
    * @return  the number of children of the node <code>parent</code>
    *
    */
   public int getChildCount (Object parent)
   {
      NodeWrapper n = (NodeWrapper)parent;
      return n.getChildCount ();
   }
   
   /** Returns the index of child in parent.  If <code>parent</code>
    * is <code>null</code> or <code>child</code> is <code>null</code>,
    * returns -1.
    *
    * @param parent a note in the tree, obtained from this data source
    * @param child the node we are interested in
    * @return the index of the child in the parent, or -1 if either
    *    <code>child</code> or <code>parent</code> are <code>null</code>
    *
    */
   public int getIndexOfChild (Object parent, Object child)
   {
      NodeWrapper n = (NodeWrapper)parent;
      return n.getIndexOfChild (child);
   }
   
   /** Returns the root of the tree.  Returns <code>null</code>
    * only if the tree has no nodes.
    *
    * @return  the root of the tree
    *
    */
   public Object getRoot ()
   {
      return this.root;
   }
   
   /** Returns <code>true</code> if <code>node</code> is a leaf.
    * It is possible for this method to return <code>false</code>
    * even if <code>node</code> has no children.
    * A directory in a filesystem, for example,
    * may contain no files; the node representing
    * the directory is not a leaf, but it also has no children.
    *
    * @param   node  a node in the tree, obtained from this data source
    * @return  true if <code>node</code> is a leaf
    *
    */
   public boolean isLeaf (Object node)
   {
      NodeWrapper n = (NodeWrapper)node;
      return n.isLeaf ();
   }
   
   /** Removes a listener previously added with
    * <code>addTreeModelListener</code>.
    *
    * @see     #addTreeModelListener
    * @param   l       the listener to remove
    *
    */
   public void removeTreeModelListener (javax.swing.event.TreeModelListener l)
   {
        treeModelListeners.removeElement(l);
   }
   
   /** Messaged when the user has altered the value for the item identified
    * by <code>path</code> to <code>newValue</code>.
    * If <code>newValue</code> signifies a truly new value
    * the model should post a <code>treeNodesChanged</code> event.
    *
    * @param path path to the node that the user has altered
    * @param newValue the new value from the TreeCellEditor
    *
    */
   public void valueForPathChanged (javax.swing.tree.TreePath path, Object newValue)
   {
      // not used
   }
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
    protected void fireTreeStructureChanged(RootWrapper oldRoot) {
        int len = treeModelListeners.size();
        
        javax.swing.event.TreeModelEvent e = new javax.swing.event.TreeModelEvent(this, 
                                              new Object[] {oldRoot});
        for (int i = 0; i < len; i++) {
            ((javax.swing.event.TreeModelListener)treeModelListeners.elementAt(i)).
                    treeStructureChanged(e);
        }
    }

   /*protected PluginManagerMBean getPM () throws Exception
   {
      return (PluginManagerMBean)ctx.lookup (this.pluginMgrJndiName);
   }*/
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
      
}
