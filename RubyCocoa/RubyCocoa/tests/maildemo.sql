create table Mailboxes (
  id          integer primary key,
  title       varchar(255) default "title"
);

create table Emails (
  id          integer primary key,
  address     varchar(255) default "test@test.com",
  subject     varchar(255) default "test subject",
  body        text,
  updated_at  datetime not null,
  mailbox_id  integer(11)
);