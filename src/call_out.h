void co_init	P((uindex, int));
bool co_new	P((object*, string*, long, int));
long co_del	P((object*, string*));
void co_timeout	P((void));
void co_call	P((void));

uindex co_count    P((void));
long   co_swaprate P((void));
