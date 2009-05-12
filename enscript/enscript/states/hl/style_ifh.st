
state style_ifh
{
  BEGIN {
    call (default_faces);

    if (color)
      {
	/* Set colors. */
	face_bold[fg_color] =		language_color ("black");
	face_italic[fg_color] =		language_color ("black");
	face_bold_italic[fg_color] =	language_color ("black");
	face_comment[fg_color] =	language_color ("DarkSeaGreen");
	face_function_name[fg_color] =	language_color ("Coral");
	face_variable_name[fg_color] =	language_color ("dark goldenrod");
	face_keyword[fg_color] =	language_color ("SeaGreen2");
	face_reference[fg_color] =	language_color ("forest green");
	face_string[fg_color] =		language_color ("Goldenrod");
	face_reference[fg_color] =	language_color ("Aquamarine");
	face_builtin[fg_color] =	language_color ("purple");
	face_type[fg_color] =		language_color ("DarkSlateGray1");
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
