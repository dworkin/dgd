The files in this directory can be used to run DGD inside a docker
container, which will contain the DGD binary and JIT object cache, but not
the LPC source tree or the snapshot files.

In contrast to the typical container setup, which depends on static images
that have to be replaced to keep up with new releases and the disclosure
of vulnerabilities, necessitating a restart of the container, this
configuration is persistent and all components (including the base
system) can be updated without restarting, and without service interruption
even for connected clients.

DGD is configured to be hotbooted into a new version after an update.  The
JIT compiler extension, gzip extension and crypto extension are included
in the container.

The container expects the following to be present in the current directory
when started:

-   server.dgd  
    a configuration file for DGD (see server.dgd.example)
-   src  
    the LPC source tree
-   state  
    where snapshots are stored

First, modify `Dockerfile` and set the `USER` argument to the ID of the user
who owns the source tree.  Then, copy your DGD config file to `server.dgd`;
lines marked with `PLACEHOLDER` in `server.dgd.example` will be replaced by
the container with appropriate container-relative values.

Then, copy `docker-compose.yml.example` to `docker-compose.yml` and change
the ports to match those in the configuration file.

Build and start the container with `docker-compose up`.  Update the
running container with `docker exec <container-id> /container/update`.
