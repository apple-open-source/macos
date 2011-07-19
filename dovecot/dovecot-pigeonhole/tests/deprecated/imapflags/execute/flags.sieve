require "imapflags";
require "fileinto";
require "mailbox";

setflag "\\draft";
fileinto :create "Set";

addflag "\\flagged";
fileinto :create "Add";

removeflag "\\draft";
fileinto :create "Remove";
