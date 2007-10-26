provider dslockstat
{
	probe mutex__acquire(long, char *, char *, int);
	probe mutex__release(long, char *, char *, int);
};
