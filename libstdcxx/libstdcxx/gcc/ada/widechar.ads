------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                             W I D E C H A R                              --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--          Copyright (C) 1992-2005 Free Software Foundation, Inc.          --
--                                                                          --
-- GNAT is free software;  you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 2,  or (at your option) any later ver- --
-- sion.  GNAT is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNAT;  see file COPYING.  If not, write --
-- to  the Free Software Foundation,  59 Temple Place - Suite 330,  Boston, --
-- MA 02111-1307, USA.                                                      --
--                                                                          --
-- As a special exception,  if other files  instantiate  generics from this --
-- unit, or you link  this unit with other files  to produce an executable, --
-- this  unit  does not  by itself cause  the resulting  executable  to  be --
-- covered  by the  GNU  General  Public  License.  This exception does not --
-- however invalidate  any other reasons why  the executable file  might be --
-- covered by the  GNU Public License.                                      --
--                                                                          --
-- GNAT was originally developed  by the GNAT team at  New York University. --
-- Extensive contributions were provided by Ada Core Technologies Inc.      --
--                                                                          --
------------------------------------------------------------------------------

--  Subprograms for manipulation of wide character sequences. Note that in
--  this package, wide character and wide wide character are not distinguished
--  since this package is basically concerned with syntactic notions, and it
--  deals with Char_Code values, rather than values of actual Ada types.

with Types; use Types;

package Widechar is

   function Length_Wide return Nat;
   --  Returns the maximum length in characters for the escape sequence that
   --  is used to encode wide character literals outside the ASCII range. Used
   --  only in the implementation of the attribute Width for Wide_Character
   --  and Wide_Wide_Character.

   procedure Scan_Wide
     (S   : Source_Buffer_Ptr;
      P   : in out Source_Ptr;
      C   : out Char_Code;
      Err : out Boolean);
   --  On entry S (P) points to the first character in the source text for
   --  a wide character (i.e. to an ESC character, a left bracket, or an
   --  upper half character, depending on the representation method). A
   --  single wide character is scanned. If no error is found, the value
   --  stored in C is the code for this wide character, P is updated past
   --  the sequence and Err is set to False. If an error is found, then
   --  P points to the improper character, C is undefined, and Err is
   --  set to True.

   procedure Set_Wide
     (C : Char_Code;
      S : in out String;
      P : in out Natural);
   --  The escape sequence (including any leading ESC character) for the
   --  given character code is stored starting at S (P + 1), and on return
   --  P points to the last stored character (i.e. P is the count of stored
   --  characters on entry and exit, and the escape sequence is appended to
   --  the end of the stored string). The character code C represents a code
   --  originally constructed by Scan_Wide, so it is known to be in a range
   --  that is appropriate for the encoding method in use.

   procedure Skip_Wide (S : String; P : in out Natural);
   --  On entry, S (P) points to an ESC character for a wide character escape
   --  sequence or to an upper half character if the encoding method uses the
   --  upper bit, or to a left bracket if the brackets encoding method is in
   --  use. On exit, P is bumped past the wide character sequence. No error
   --  checking is done, since this is only used on escape sequences generated
   --  by Set_Wide, which are known to be correct.

   procedure Skip_Wide (S : Source_Buffer_Ptr; P : in out Source_Ptr);
   --  Similar to the above procedure, but operates on a source buffer
   --  instead of a string, with P being a Source_Ptr referencing the
   --  contents of the source buffer.

   function Is_Start_Of_Wide_Char
     (S : Source_Buffer_Ptr;
      P : Source_Ptr) return Boolean;
   --  Determines if S (P) is the start of a wide character sequence

   function Is_UTF_32_Letter (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Letter);
   --  Returns true iff U is a letter that can be used to start an identifier.
   --  This means that it is in one of the following categories:
   --    Letter, Uppercase (Lu)
   --    Letter, Lowercase (Ll)
   --    Letter, Titlecase (Lt)
   --    Letter, Modifier  (Lm)
   --    Letter, Other     (Lo)
   --    Number, Letter    (Nl)

   function Is_UTF_32_Digit (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Digit);
   --  Returns true iff U is a digit that can be used to extend an identifer,
   --  which means it is in one of the following categories:
   --    Number, Decimal_Digit (Nd)

   function Is_UTF_32_Line_Terminator (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Line_Terminator);
   --  Returns true iff U is an allowed line terminator for source programs,
   --  which means it is in one of the following categories:
   --    Separator, Line (Zl)
   --    Separator, Paragraph (Zp)
   --  or that it is a conventional line terminator (CR, LF, VT, FF)

   function Is_UTF_32_Mark (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Mark);
   --  Returns true iff U is a mark character which can be used to extend
   --  an identifier. This means it is in one of the following categories:
   --    Mark, Non-Spacing (Mn)
   --    Mark, Spacing Combining (Mc)

   function Is_UTF_32_Other (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Other);
   --  Returns true iff U is an other format character, which means that it
   --  can be used to extend an identifier, but is ignored for the purposes of
   --  matching of identiers. This means that it is in one of the following
   --  categories:
   --    Other, Format (Cf)

   function Is_UTF_32_Punctuation (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Punctuation);
   --  Returns true iff U is a punctuation character that can be used to
   --  separate pices of an identifier. This means that it is in one of the
   --  following categories:
   --    Punctuation, Connector (Pc)

   function Is_UTF_32_Space (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Space);
   --  Returns true iff U is considered a space to be ignored, which means
   --  that it is in one of the following categories:
   --    Separator, Space (Zs)

   function Is_UTF_32_Non_Graphic (U : Char_Code) return Boolean;
   pragma Inline (Is_UTF_32_Non_Graphic);
   --  Returns true iff U is considered to be a non-graphic character,
   --  which means that it is in one of the following categories:
   --    Other, Control (Cc)
   --    Other, Private Use (Co)
   --    Other, Surrogate (Cs)
   --    Other, Format (Cf)
   --    Separator, Line (Zl)
   --    Separator, Paragraph (Zp)
   --
   --  Note that the Ada category format effector is subsumed by the above
   --  list of Unicode categories.

   function UTF_32_To_Upper_Case (U : Char_Code) return Char_Code;
   pragma Inline (UTF_32_To_Upper_Case);
   --  If U represents a lower case letter, returns the corresponding upper
   --  case letter, otherwise U is returned unchanged. The folding is locale
   --  independent as defined by documents referenced in the note in section
   --  1 of ISO/IEC 10646:2003

end Widechar;
