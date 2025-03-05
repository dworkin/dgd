# DGD

This file was written for release 1.7 of DGD, Dworkin's Game Driver.

DGD is a rewrite from scratch of the LPMud server.  It runs on Windows, MacOS
and many versions of Unix.

[DGD source repository](https://github.com/dworkin/dgd)

[DGD's home page](https://www.dworkin.nl/dgd)

DGD offers JIT compilation support.  This requires the
[JIT extension module](https://github.com/dworkin/lpc-ext).

Alongside DGD, there is Hydra.  Hydra is fully compatible with DGD, but offers
acceleration for multi-core systems.  Hydra also supports JIT compilation,
using the same extension module as DGD.

[Hydra's home page](https://www.dworkin.nl/hydra-tech)

### PLEASE NOTE

DGD 1.7 is backward compatible with snapshots from DGD 1.5.9 and later, only.
To upgrade to DGD 1.7, first upgrade to the latest 1.5.x version, recompile
all objects, and create a non-incremental snapshot.


This distribution is organized as follows:

-   bin  
    Installation binaries will be created here (Unix).
-   container  
    Example files for running DGD in a container.
-   doc  
    Documentation on how to compile and run DGD.
-   lpc-doc  
    LPC documentation, still very incomplete.
-   src  
    Where the source code of DGD resides, and where you issue your `make`
    command on Unix.
-   src/host  
    Subdirectories for various platform-dependent files.
