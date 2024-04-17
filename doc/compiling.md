## A few useful notes for compiling DGD

DGD's build process is very simple, and doesn't have
a configure script.  Platforms are detected in
`src/Makefile`.  You will need a C++ compiler and yacc or
a yacc-compatible parser generator.

DGD can be compiled on Unix-like systems and on Windows.  See
`src/host/win32/README.md` for additional information regarding
the Windows build.

There are a few things you can enable/disable by adding the
proper defines to the Makefile on Unix systems, or to your
project on Visual Studio.

-   LARGENUM  
    64 bit integers and floats.
-   SLASHSLASH  
    C++ style // comments in LPC code.
-   SIMFLOAT
    Simulated floats.  Not a macro but a argument for `make`, `make SIMFLOAT=1`.
-   NOFLOAT  
    Compile without using host floats (requires simulated floats).
-   CLOSURES  
    Function pointers, implemented as a builtin type.  See `doc/builtin.md` for
    information about adding builtin types.  CLOSURES allows taking the
    address of a function with `&func()`, and calling the function that a
    function pointer refers to with `(*func)()`.  When creating a pointer to
    a function, function arguments may be included: `&func("foo")`.  More
    function arguments can be added when the function is called, or in
    constructing a new pointer: `&(*func)("bar")`
