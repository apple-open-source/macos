/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

using System;
using System.Windows.Forms;

namespace org.jboss.net.samples.store
{
	/** 
	 * <summary>
	 * Controller for Reading and Manipulating the BusinessModel 
	 * through a ListView.
	 * </summary>
	 * @author jung
	 * @created 21.03.2002
	 * @version $Revision: 1.1 $
	 */

	public abstract class ListController
	{
		#region Attributes
		/** the parent form */
		protected SampleView parent;
		/** the list view to which we belong */
		protected ListView view;
		/** the objects that we are hosting */
		Object[] items;
		/** the listview items that are shown from them */
		ListViewItem[] listItems;
		#endregion

		/** creates a new ListController for the given view */
		public ListController(SampleView parent,ListView view)
		{
			this.parent=parent;
			this.view=view;
		}


		#region Abstract Interface to Fill With Concrete Service Interaction

		/** create a new item **/
		public abstract Object create(); 
		/** creates a list view item for the given object */
		public abstract ListViewItem createListItem(Object target);
		/** retrieve the whole set of objects **/
		public abstract Object[] retrieveObjects(); 
		/** deletes a single item */
		public abstract void delete(Object target);
		
		#endregion

		#region Concrete Implementation of ListView Treatment

		/** is called to re-arrange the view **/
		public virtual bool arrange() 
		{
			view.Columns.Clear();
			return false;
		}

		/** no editor */
		public virtual void edit(Object target) 
		{
		}

		/** no update */
		public virtual void update(Object target) 
		{
		}

		// is called whenever the view must be update
		public void populate() 
		{
			view.Items.Clear();
			items=retrieveObjects();
			listItems=new ListViewItem[items.Length];
			for(int count=0;count<items.Length;count++) 
			{
				listItems[count]=createListItem(items[count]);
			}
			view.Items.AddRange(listItems);
		}
		
		// adds a new item
		public void addNew()
		{
			Object[] newItems=new Object[items.Length+1];
			items.CopyTo(newItems,0);
			newItems[items.Length]=create();
			ListViewItem[] newListItems=new ListViewItem[items.Length+1];
			listItems.CopyTo(newListItems,0);
			newListItems[items.Length]=createListItem(newItems[items.Length]);
			view.Items.Add(newListItems[items.Length]);
			items=newItems;
			listItems=newListItems;
		}

		// deletes a few particular items
		public void delete(ListView.SelectedListViewItemCollection listItemsToDelete) 
		{
			Object[] newItems=new Object[items.Length-listItemsToDelete.Count];
			ListViewItem[] newListItems=new ListViewItem[items.Length-listItemsToDelete.Count];
			int increment=0;

			for(int count=0;count<listItems.Length;count++) 
			{
				if(listItemsToDelete.Contains(listItems[count])) 
				{
					delete(items[count]);
					view.Items.Remove(listItems[count]);
				} 
				else 
				{
					newItems[increment]=items[count];
					newListItems[increment]=listItems[count];
					increment++;
				}
			}

			items=newItems;
			listItems=newListItems;
		}

		public void edit(ListView.SelectedListViewItemCollection 
			listItemsToEdit) 
		{
			for(int count=0;count<listItems.Length;count++) 
			{
				if(listItemsToEdit.Contains(listItems[count])) 
				{
					edit(items[count]);
				}
			}
		}

		public void processUpdate(Object target) 
		{
			bool found=false;
			for(int count=0;count<items.Length && !found;count++) 
			{
				if(items[count].Equals(target)) 
				{
					found=true;
					update(target);
					ListViewItem newView=createListItem(target);
					view.Items.Insert(count,newView);
					view.Items.Remove(listItems[count]);
				}
			}
		}

		#endregion

	} // ListController
} //Namespace