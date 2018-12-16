<p align="center">
<img src="https://raw.githubusercontent.com/hpjansson/chafa/master/docs/chafa-logo.gif" />
</p>

Chafa is a command-line utility that converts all kinds of images, including
animated GIFs, into ANSI/Unicode character output that can be displayed in a
terminal.

It is highly configurable, with support for alpha transparency and multiple
color modes and color spaces, combining selectable ranges of Unicode
characters to produce the desired output.

The core functionality is provided by a C library with a public,
well-documented API.

It has [official web pages](https://hpjansson.org/chafa/) and [C API
documentation](http://hpjansson.org/chafa/ref/) online.

## Installing with Package manager

### Arch Linux

Chafa has been added to the [community] repository. Use pacman to install:

```sh
$ sudo pacman -S chafa
```

### Debian testing/unstable

Chafa has been packaged for Debian. Issue the following command to install:

```sh
$ sudo apt install chafa
```

For supported Debian releases, please see the
[package status page](https://tracker.debian.org/pkg/chafa).

### Fedora

Chafa has been packaged for Fedora. Issue the following command to install:

```sh
$ sudo dnf install chafa
```

## Installing from tarball

You will need GCC, make and the GLib development package installed to
compile Chafa from a release tarball. If you want to build the
command-line tool `chafa` and not just the library, you will
additionally need the ImageMagick development packages.

Prebuilt documentation is included in the release tarball, and you
do not need gtk-doc unless you want to make changes/rebuild it.

After unpacking, cd to the toplevel directory and issue the following
shell commands:

```sh
$ ./configure
$ make
$ sudo make install
```

## Installing from git repository

You will need GCC, make, Autoconf, Automake, Libtool and the GLib
development package installed to compile Chafa from its git repository. If
you want to build the command-line tool `chafa` and not just the library,
you will additionally need the ImageMagick development packages.

If you want to build documentation, you will also need gtk-doc.

Start by cloning the repository:

```sh
$ git clone https://github.com/hpjansson/chafa.git
```

Then cd to the toplevel directory and issue the following shell commands:

```sh
$ ./autogen.sh
$ make
$ sudo make install
```

## Further reading

For tarball releases, additional documentation, etc. see [the official
web pages](https://hpjansson.org/chafa/).

<p align="center">
<img src="https://hpjansson.org/chafa/img/example-2.gif" />
</p>
