# define ITERATIONS	25000

# ifdef __DGD__
# define write(str)	send_message(str)
# endif


int *start, *end;

void call(string function)
{
    call_other(this_object(), function);
    write("rusage of " + function + ": " +
	  (end[0] - start[0] + end[1] - start[1]) +
	  "\n");
}

/* empty loop */

void test1()
{
   int j;

   start = rusage();
   for (j = 0; j < ITERATIONS; j++) {
   }
   end = rusage();
}

void yy() { }

void test2()
{
   int j;

   start = rusage();
   for (j = 0; j < ITERATIONS; j++) {
      yy();
   }
   end = rusage();
}

void test3()
{
  int j;

   start = rusage();
  for (j = 0; j < ITERATIONS; j++) {
     this_object()->yy();
  }
   end = rusage();
}

void test4()
{
  string s;
  int j;

   start = rusage();
  for (j = 0; j < ITERATIONS; j++) {
     s = "xxxxxx";
     s += s;
  }
   end = rusage();
}

void test5()
{
  string *s;
  int j;

   start = rusage();
  for (j = 0; j < ITERATIONS; j++) {
    s = allocate(10);
    s += s;
  }
   end = rusage();
}

void test6()
{
  int j;

  j = ITERATIONS;
   start = rusage();
  while (j--) ;
   end = rusage();
}

void test7()
{
  int j;

   start = rusage();
  for (j = 0; j < ITERATIONS; j++) {
     iyy();
  }
   end = rusage();
}

void test8()
{
  int i, j;

   start = rusage();
  for (j = 0; j < ITERATIONS; j++) {
     /*
      * Should be able to optimize following expression quite much. For
      * example, produced code should not contain any divisions,
      * multiplications or modulos. All them can be done with
      * shifts and with ands.
      */
      i = (i / 4) + (i % 256) + (i / 4) - 1 * i + i * 4 + i / 1;
  }
   end = rusage();
}

void test9()
{
  int i, j;
  string str;

   start = rusage();
  for (j = 0; j < ITERATIONS; j++) {
  /*
   * Both these expressions can be calculated at compile time
   */
    str = "Test" + " " + "string" + " " + "which" + " " + "divided"
      + " " + "to" + " " + "smaller" + " " + "parts.";
    i = (100 * 34) / 8 + (136 % 100) * 4 - 1000;
  }
   end = rusage();
}

void test11()
{
  int j;
  mapping map;

  map = ([]);
   start = rusage();
  for (j = 0; j < 2500; j++) {
     map[j] = j;
  }
   end = rusage();
}
