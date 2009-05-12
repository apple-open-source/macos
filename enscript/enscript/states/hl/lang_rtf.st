/*
 * Definition for RTF output
 * Kevin Grover, <grover@wizard.com>
 */

state lang_rtf
{
  BEGIN {
    sub rtf_color_map()
      {
	local i;

	print ("{\\colortbl;");
	for (i = 0; i < length (rgb_values); i = i + 1)
	  print (sprintf ("\\red%d\\green%d\\blue%d;",
			  rgb_values[i][1],
			  rgb_values[i][2],
			  rgb_values[i][3]));

	print ("}\n");

	return;
      }

    sub language_color (name)
      {
	local idx;

	idx = color_index (name);
	if (idx < 0)
	  panic ("unknown color `", name, "'");

	return sprintf("\\cf%d", idx + 1);
      }

    LANGUAGE_SPECIALS = /[\\{}\n]/;

    sub language_print (str)
      {
	str = regsuball (str, /\\\\/, "\\\\");
	str = regsuball (str, /{/, "\\{");
	str = regsuball (str, /}/, "\\}");
	str = regsuball (str, /\n/, "\\line\n");
	print (str);
      }

    sub language_symbol (symbol)
      {
	return false;
      }

    sub header ()
      {
	local i;

	if (current_input_file == 1)
	  {
	    print ("{\\rtf\\ansi\\deff0\n");
	    print ("{\\fonttbl{\\f0\\fswiss Courier New;}}\n");
	    rtf_color_map();
	  }
      }

    sub trailer ()
      {
	if (current_input_file == int (num_input_files))
	  print ("}\n");
      }

    sub color_on (name)
      {
	print ("{", name, " ");
      }

    sub color_off ()
      {
	print ("}");
      }

    sub face_on (face)
      {
	if (face[boldp] || face[italicp])
	  {
	    print ("{");
	    if (face[boldp])
	      print ("\\b");
	    if (face[italicp])
	      print ("\\i");
	    print (" ");
	  }

	if (face[fg_color])
	  color_on (face[fg_color]);
      }

    sub face_off (face)
      {
	if (face[fg_color])
	  color_off ();
	if (face[boldp] || face[italicp])
	  print ("}");
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
