/// <summary> Copyright (C) 2004  Free Software Foundation, Inc.
/// *
/// Author: Alexander Gnauck AG-Software
/// *
/// This file is part of GNU Libidn.
/// *
/// This library is free software; you can redistribute it and/or
/// modify it under the terms of the GNU Lesser General Public License
/// as published by the Free Software Foundation; either version 2.1 of
/// the License, or (at your option) any later version.
/// *
/// This library is distributed in the hope that it will be useful, but
/// WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
/// Lesser General Public License for more details.
/// *
/// You should have received a copy of the GNU Lesser General Public
/// License along with this library; if not, write to the Free Software
/// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
/// USA
/// </summary>
using System;
using System.Collections;

namespace gnu.inet.encoding.misc
{
    /// <summary>
    /// HashSet class.
    /// </summary>    
    public class HashSet : ArrayList
    {
        public HashSet() : base()
        {
        }

        public HashSet(System.Collections.ICollection c)
        {
            this.AddAll(c);
        }

        public HashSet(int capacity) : base(capacity)
        {
        }

        /// <summary>
        /// Adds a new element to the ArrayList if it is not already present.
        /// </summary>		
        /// <param name="obj">Element to insert to the ArrayList.</param>
        /// <returns>Returns true if the new element was inserted, false otherwise.</returns>
        new public virtual bool Add(System.Object obj)
        {
            bool inserted;

            if ((inserted = this.Contains(obj)) == false)
            {
                base.Add(obj);
            }

            return !inserted;
        }

        /// <summary>
        /// Adds all the elements of the specified collection that are not present to the list.
        /// </summary>
        /// <param name="c">Collection where the new elements will be added</param>
        /// <returns>Returns true if at least one element was added, false otherwise.</returns>
        public bool AddAll(System.Collections.ICollection c)
        {
            System.Collections.IEnumerator e = new System.Collections.ArrayList(c).GetEnumerator();
            bool added = false;

            while (e.MoveNext() == true)
            {
                if (this.Add(e.Current) == true)
                    added = true;
            }

            return added;
        }

        /// <summary>
        /// Returns a copy of the HashSet instance.
        /// </summary>		
        /// <returns>Returns a shallow copy of the current HashSet.</returns>
        public override System.Object Clone()
        {
            return base.MemberwiseClone();
        }
    }
}