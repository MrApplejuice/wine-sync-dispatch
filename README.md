# Wine sync dispatch

Allows one to invoke Linux-based programs synchronously from wine.

## wine-dispatcher

The receiver for invocations from wine-pack executables.

## wine-pack

Packer creates a "behave alike" executable that links to a linux executable.

Usage:

    pack [--socket=address:port] [wine-exe] [linux-exe] [baked-in-args ...]
    
wine-exe: The Wine-path to the wine executable that should be created
linux-exe: The linux-path to the linux executable that wine-exe should link to.
  The path can be either absolute or relative to the place that wine-dispatcher
  is run in.
baked-in-args: it is possible to bake-in arguments that linux-exe should be 
  called with. Baked-in arguments will be passed as first arguments to the 
  internal executable followed by any positional arguments given to wine-exe.
socket: the socket through which this executable should try to contact wn

The resulting wine-exe will attempt call linux-exe by contacting wine-dispatcher
through a local websocket. For discovery of the wine-dispatcher node the
following strategy is used:

1. Check any configured address:port given to pack during the packing operation.
2. Read the environment variable WINE_DISPATCHER_ADDRESS=address:port
3. Read the configuration file at the magic location c:\wine-dispatcher.conf

## wine-dispatcher.conf

Lines starting with a '#' are ignored. Valid values of the configuration file
are:

DISPATCHER_ADDRESS=address:port

