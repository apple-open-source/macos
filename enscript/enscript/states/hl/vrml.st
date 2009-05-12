/**
 * Name: vrml
 * Description: Virtual Reality Modeling Language (VRML97)
 *
 *   Typesets UTF-8 encoded VRML files including inline scripts
 *   (JavaScript or VrmlScript) using Encript's JavaScript mode.
 *
 *   Note: Some VRML browsers allow use of double quotes in
 *   defining string literals inside inlined scripts.  Doing so
 *   breaks the logic programmed here.  Using single quotes
 *   (i.e. ECMAScript spec-compliant string literals) works fine.
 *
 * Author: James Sørlie <sorliej@imm.dtu.dk>
 */

state vrml extends HighlightEntry
{
  BEGIN {
    require_state (javascript);
  }

  /* Comments. */
  /#/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* Inline Script Protocols. */
  /\"javascript:|\"vrmlscript:/ {
    reference_face (true);
    language_print ($0);
    reference_face (false);
    from_vrml = 1;
    call (javascript_internal);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* UTF-8 header text. */
  /^#VRML V2.0/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    reference_face (false);
  }

  /* Built-ins nodes:
     http://www.vrml.org/Specifications/VRML97/part1/nodesRef.html

     (build-re '(Introduction Anchor Appearance AudioClip Background
     Billboard Box Collision Color ColorInterpolator Cone Coordinate
     CoordinateInterpolator Cylinder CylinderSensor DirectionalLight
     ElevationGrid Extrusion Fog FontStyle Group ImageTexture
     IndexedFaceSet IndexedLineSet Inline LOD Material MovieTexture
     NavigationInfo Normal NormalInterpolator OrientationInterpolator
     PixelTexture PlaneSensor PointLight PointSet PositionInterpolator
     ProximitySensor ScalarInterpolator Script Shape Sound Sphere
     SphereSensor SpotLight Switch Text TextureCoordinate TextureTransform
     TimeSensor TouchSensor Transform Viewpoint VisibilitySensor
     WorldInfo))
  */
  /\b(A(nchor|ppearance|udioClip)|B(ackground|illboard|ox)\
|C(o(l(lision|or(|Interpolator))|ne|ordinate(|Interpolator))\
|ylinder(|Sensor))\
|DirectionalLight|E(levationGrid|xtrusion)|Fo(g|ntStyle)|Group\
|I(mageTexture|n(dexed(FaceSet|LineSet)|line|troduction))|LOD\
|M(aterial|ovieTexture)|N(avigationInfo|ormal(|Interpolator))\
|OrientationInterpolator\
|P(ixelTexture|laneSensor|o(int(Light|Set)|sitionInterpolator)\
|roximitySensor)\
|S(c(alarInterpolator|ript)|hape|ound|p(here(|Sensor)|otLight)|witch)\
|T(ext(|ure(Coordinate|Transform))|imeSensor|ouchSensor|ransform)\
|Vi(ewpoint|sibilitySensor)|WorldInfo)\b/ {
    builtin_face (true);
    language_print ($0);
    builtin_face (false);
  }

  /* Keywords.
     http://www.vrml.org/Specifications/VRML97/part1/grammar.html#TableA.1 */
  /\b(DEF|EXTERNPROTO|FALSE|IS|NULL|PROTO|ROUTE|T(O|RUE)|USE|\
e(vent(In|Out)|xposedField)|field)\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }

  /* Types.
     http://www.vrml.org/Specifications/VRML97/part1/grammar.html#Fields */
  /\b(MF(Color|Float|Int32|Node|Rotation|String|Time|Vec(2f|3f))|\
SF(Bool|Color|Float|Image|Int32|Node|Rotation|String|Time|Vec(2f|3f)))\b/ {
    type_face (true);
    language_print ($0);
    type_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
