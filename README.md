xmon - graphical system monitor
===============================

![xmon](http://nuclear.mutantstargoat.com/sw/xmon/img/xmon-shot002.png)

Xmon is a simple graphical system monitoring utility for UNIX systems with X11.

Xmon strives to:
  - Run on as many UNIX systems as possible.
  - Be as lighweight and efficient as possible.
  - Run well on X servers with reduced colors.
  - Run well both locally and over the network.
  - Be as configurable as possible.

Currently xmon supports: Linux, IRIX, and MacOS X.


History
-------
Xmon started as an experiment to find a good way to present CPU usage of more
than a handful of cores. I was disappointed by existing system monitors which
either show the average usage (which for a 32-thread processor will be mostly
blank if you're not maxing most cores), or attempt to fit N different CPU
graphs onto my screen which are just meaningless noise and clutter.

The idea behind Xmon's CPU usage display, was to use a waterfall plot display,
similar to plots often used in frequency analysis. The horizontal axis is split
among the CPUs/cores, the vertical axis is time, and the plot intensity/color is
a function of the usage of the corresponding core.

I got the idea to use a waterfall plot from a comment Dave Plummer made, while
interviewing Dave Cutler, about potentially using a "heatmap" to visualize CPU
utilization for hundreds of cores/threads. I ended up going with a waterfall
plot instead of a heatmap, because it also conveys utilization over time.


License
-------
Copyright (C) 2025 John Tsiombikas <nuclear@mutantstargoat.com>

Xmon is Free Software. Feel free to use, modify, and/or redistribute under the
terms of the GNU General Public License v3, or at your option any later version
published by the Free Software Foundation. See COPYING for details.


Build instructions
------------------

### GNU/Linux

Just run `make`, and `make install` if you wish to install xmon system-wide. The
default installation prefix is `/usr/local`, which can be changed by modifying
the first line of `GNUmakefile`.

### IRIX

To build on IRIX with MIPSPro C and the native make utility, run `make -f
Makefile.sgi`, and then `make -f Makefile.sgi install` to install xmon
system-wide. The default installation prefix is `/usr/local`, which can be
changed by modifying the first line of `Makefile.sgi`.

To build with gcc and GNU make, run `gmake CC=gcc`, then `gmake install` to
install. Again, to change the installation prefix, edit the first line of
`GNUmakefile`.

### MacOS X

Make sure XQuartz is installed, and that you have all the necessary Xlib
headers. You can install both with homebrew: `brew install xquartz libx11`.
Then just run `make` to compile, and `make install` to install.


Notes
-----

### IRIX setuid root

On IRIX xmon is installed as setuid-root, to be able to access kernel memory to
read the load average. Root priviledges are dropped immediately after /dev/kmem
is opened. See `load_init` in `src/irix/load.c`.

### IRIX network traffic unit is packets

On IRIX xmon can't compute traffic in bytes. Only packet counters are
available. So all displayed numbers are packets or packets / sec.
