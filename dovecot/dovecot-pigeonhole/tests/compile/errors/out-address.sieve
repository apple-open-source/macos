require "vacation";

# Error 

redirect "@wrong.example.com";
redirect "error";
redirect "error@";
redirect "Stephan Bosch error@example.org";
redirect "Stephan Bosch <error@example.org";
redirect " more error @  example.com  ";
redirect "@";
redirect "<>";
redirect "Error <";
redirect "Error <stephan";
redirect "Error <stephan@";
redirect "stephan@example.org,tss@example.net";
redirect "stephan@example.org,%&^&!!~";

vacation :from "Error" "Ik ben er niet.";

# Ok

redirect "Ok Good <stephan@example.org>";
redirect "ok@example.com";
redirect " more  @  example.com  ";

vacation :from "good@voorbeeld.nl.example.com" "Ik ben weg!";
