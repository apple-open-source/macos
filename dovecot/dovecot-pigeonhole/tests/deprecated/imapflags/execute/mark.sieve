require "imapflags";
require "fileinto";
require "mailbox";

mark;

fileinto :create "Marked";

unmark;

fileinto :create "Unmarked";
