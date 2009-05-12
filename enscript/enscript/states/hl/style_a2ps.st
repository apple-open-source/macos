
state style_a2ps
{
  BEGIN {
    call (default_faces);

    /* Fonts. */
    face_type =				make_face (italic_font, 0, 1);
    face_string = 			make_face ("Times-Roman", 0, 0);

    if (color)
      {
	/* Set colors. */
	/* These are incomplete. */
	face_bold[fg_color] =		language_color ("black");
	face_italic[fg_color] =		language_color ("black");
	face_bold_italic[fg_color] =	language_color ("black");
	face_comment[fg_color] =	language_color ("red");
	face_function_name[fg_color] =	language_color ("blue");
	face_variable_name[fg_color] =	language_color ("dark goldenrod");
	face_keyword[fg_color] =	language_color ("purple");
	face_reference[fg_color] =	language_color ("cadet blue");
	face_string[fg_color] =		language_color ("forest green");
	face_builtin[fg_color] =	language_color ("orchid");
      }
    else
      {
	/* Highlighting for b&w / grayscale outputs. */
	face_comment[bg_color] =		language_color ("gray90");
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
