/// <summary> Copyright (C) 2004, 2005  Free Software Foundation, Inc.
/// *
/// Author: Alexander Gnauck AG-Software
/// *
/// This file is part of GNU Libidn.
/// *
/// This program is free software; you can redistribute it and/or
/// modify it under the terms of the GNU General Public License as
/// published by the Free Software Foundation; either version 2 of the
/// License, or (at your option) any later version.
/// *
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
/// General Public License for more details.
/// *
/// You should have received a copy of the GNU General Public License
/// along with this program; if not, write to the Free Software
/// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
/// 02111-1307 USA.
/// </summary>

using System;

namespace gnu.inet.encoding
{	
	public class StringprepException:System.Exception
	{
		public static System.String CONTAINS_UNASSIGNED = "Contains unassigned code points.";
		public static System.String CONTAINS_PROHIBITED = "Contains prohibited code points.";
		public static System.String BIDI_BOTHRAL = "Contains both R and AL code points.";
		public static System.String BIDI_LTRAL = "Leading and trailing code points not both R or AL.";
		
		public StringprepException(System.String m) : base(m)
		{
		}
	}
}