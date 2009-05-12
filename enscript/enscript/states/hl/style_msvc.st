/* MS Visual C -styled style */

state style_msvc
{
  BEGIN {
    /* This is really boring style. */
    call (default_faces);

    face_bold 		= make_face (bold_font, 1, 0);
    face_italic		= make_face (italic_font, 0, 1);
    face_bold_italic	= make_face (bold_italic_font, 1, 1);
    face_function_name	= make_face (font, 0, 0);
    face_variable_name	= make_face (font, 0, 0);
    face_string		= make_face (font, 0, 0);
    face_builtin	= make_face (font, 0, 0);

    /* These are the only faces which are highlighted. */
    if (color)
      {
	face_comment			= make_face (font, 0, 0);
	face_comment[fg_color] 		= language_color("forest green");

	face_keyword			= make_face (font, 0, 0);
	face_keyword[fg_color] 		= language_color("blue");

	face_type			= make_face (font, 0, 0);
	face_type[fg_color] 		= language_color("blue");

	face_reference			= make_face (font, 0, 0);
	face_reference[fg_color]	= language_color("blue");
      }
    else
      {
	face_comment	= make_face (italic_font, 1, 0);
	face_keyword	= make_face (bold_font, 1, 0);
	face_type	= make_face (bold_font, 1, 0);
	face_reference	= make_face (bold_font, 1, 0);
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
