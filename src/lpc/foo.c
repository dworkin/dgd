inherit "/bar";

void foo()
{
    int i;

    for (i = 0; i < 10; i++) {
	send_message("Hello " + "World\n");
    }
}
