		An overview of the kernel library for DGD


1. Motivation

The kernel library was written to solve the technical problems encountered
when writing a mudlib for users who will have programming access.  It deals
with resource control, file security and user management, and offers basic
functionality in the form of events.  The library is designed to be fully
configurable, and should not have to be modified for use on any system.  It
can be used for both persistent and non-persistent systems.

Throughout this document, a game mudlib point of view will be taken, but the
kernel library can be used for any type of multi-user system.


2. Directory structure

The kernel library itself resides in /kernel.  Subdirectories are:

    /kernel/data
		    This is where kernel information is saved in non-persistent
		    systems, notably access levels and passwords for
		    programmers.
    /kernel/lib
		    Inheritable objects, such as the auto object.  None of
		    these objects can be inherited directly by programmers.
    /kernel/lib/api
		    APIs for kernel manager objects.
    /kernel/obj
		    All cloned kernel objects, such as the default user object,
		    reside in this directory.
    /kernel/sys
		    Kernel manager objects, such as the driver object, access
		    manager, resource manager, object registry, etc.

The kernel imposes the following directory structure on the rest of the
system:

    /doc
		    Documentation.
    /doc/kernel
		    Documentation about the kernel library.
    /include
		    System include files.
    /include/kernel
		    Kernel include files.
    /usr
		    The programmers' directories are subdirectories of /usr.
    /usr/System
		    The system directory, which defines the basic behaviour
		    of the mudlib above the kernel level.  Files in this
		    directory have special permissions and may inherit from
		    /kernel/lib.


2.1. Restrictions on use of objects

The kernel imposes strict limits on the use of objects. An inherited object
can never itself be accessed as an object.  It is not possible to call a
function in it, and special functionality exists to compile and destruct it.

Inheritable objects must have "lib" as a path component.  Clonable objects
must have "obj" as a path component.  "lib" overrides "obj", so the file
/usr/System/lib/foo/obj/bar.c is inheritable, but not clonable.  Objects
with neither "lib" nor "obj" in their path are not clonable or inheritable,
and can be used for such things as rooms or manager objects.


3. Resource control

The kernel library includes a system to keep track and impose limits on
resource usage.  A "resource" can be anything of which there is a limited
supply, such as ticks, amount of objects, or space used by files.  The
resource control system is generic, and new resources can be defined or 
removed on the fly.

Resource information is maintained per programmer.  Global limits can be
imposed, and exceptions can be made.  Programmers themselves can create
new resources.


4. File security

There are 3 access levels: read, write, and access-granting.  By default,
everyone has read access in all directories outside /usr.  Every object in
/kernel and /usr/System has global access.  Programmers have access-granting
access to their own directory.  A programmer's objects have the same global
read access as the programmer, the same write access in the programmer's
directory, and can clone and inherit from directories where they have read
access.  Objects neither in /usr nor in /kernel only have read access in the
usual directories, and cannot compile or clone new objects at all.

A programmer's access can be changed: administrators have access-granting
access to the root directory.  However, the programmer's access has no effect
on the access of the programmer's objects.


5. Events

An event is a way to broadcast a message to listeners.  Events are
processed using the resource limits of the receiving objects.

The number of events that an object is subscribed to is a resource,
controlled by the resource manager.


6. Application Programmer Interfaces

Each kernel manager object has a corresponding inheritable API.  The API
provides functions which, when called, are routed to the manager object.
The manager object checks each call to make sure it is called by the API,
only.

The manager APIs are inheritable only by objects in /usr/System.  Calls
are unrestricted, so any security must be provided by the inheriting
object.


7. User management

The kernel library provides a bare-bones user manager and user object.
The user object provides basic communication commands for unprivileged
users, development commands for users with programming access, and
user and resource management commands for administrators.  Both the
user manager and the user object are minimal, and intended to be extended
or replaced altogether.

Initially, users can login on either a telnet or binary port.  The only
existing user is "admin", who has administrator access.  Using the
"admin" account, other users can be granted programming access.


8. Standard wiztool

The kernel library includes a "wiztool", an object which defines the basic
developer commands for programmers.  The wiztool is inheritable, and can
thus be used as the basis for a more advanced development tool.


9. Extending the kernel library

The kernel library was written in such a way that it can easily be extended
without modifying any kernel objects.  The user manager and driver object
contain hooks that allow routing of some calls to different objects.

When you begin building, start with making a new user object.  This object
should reside in /usr/System/obj.  The user object must inherit
/kernel/lib/user.  Next, create a telnet connection manager which can be
installed using "/kernel/sys/userd"->set_telnet_manager(obj).  Finally,
create /usr/System/initd.c which handles system-specific initialization
(for instance, the telnet connection manager must be installed from its
create() function).

If you want your mud to be persistent, activate the define for
SYS_PERSISTENT in /include/std.h.

Next step: reboot.  If everything works, you can start extending your
user object, over which you now have full control.


10. Backdoor

Even if a different binary connections manager has been installed, the
user "admin" can still login on the first binary port.  To disable this
backdoor, change the file /kernel/data/admin.pwd to contain this line:

    password "*"

This will disable access for "admin".  If, at some time in the future,
things have gone so thoroughly wrong that nobody can login in the
ordinary way anymore, remove the file /kernel/data/admin.pwd and login
as "admin" on the first binary port.
