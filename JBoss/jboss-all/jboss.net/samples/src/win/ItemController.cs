/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

using System;
using JBoss_Net_Sample.item;
using System.Windows.Forms;

namespace org.jboss.net.samples.store
{
	/** 
	 * <summary>
	 * Controller for Reading and Manipulating Items through a ListView.
	 * </summary>
	 * @author jung
	 * @created 21.03.2002
	 * @version $Revision: 1.1 $
	 */

	public class ItemController : ListController
	{
		/** the attached service runs in stateless mode */
		ItemServiceService itemService=new ItemServiceService();

		/** creates a new item controller for the given view */
		public ItemController(SampleView parent,ListView view) : base(parent,view)
		{
		}


		/** create a new item */
		public override Object create() 
		{
			ItemDialogue myDialog=new ItemDialogue();
			myDialog.ShowDialog(parent);
			return itemService.create(myDialog.getText());
		}


		/** creates a list view item for the given object */
		public override ListViewItem createListItem(Object target) 
		{
			return new ListViewItem(((Item) target).name,6);
		}

		
		/** retrieve an initial set of objects */
		public override Object[] retrieveObjects() 
		{
			return itemService.findAll();
		}

		
		/** deletes a single item */
		public override void delete(Object target) 
		{
			itemService.delete((Item) target);
		}

		
		/** is called to re-arrange the view */
		public override bool arrange()  
		{
			base.arrange();
			view.Columns.Add("Item", 350, System.Windows.Forms.HorizontalAlignment.Left);
			return false;
		}

	} // ItemController
} // namespace
