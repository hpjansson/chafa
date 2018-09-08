<p align="center">
<img src="https://raw.githubusercontent.com/hpjansson/chafa/master/docs/chafa-logo.gif" />
</p>

Chafa is a command-line utility that converts all kinds of images, including
animated image formats like GIFs, into ANSI/Unicode character output that can
be displayed in a terminal.

It is highly configurable, with support for alpha transparency and multiple
color modes and color spaces, combining a range of Unicode characters for
optimal output.

The core functionality is provided by a C library with a public,
well-documented API.

## Installing with Package manager

### Debian testing/unstable

chafa has been packaged for Debian. Issue the following command to install:

```
$ sudo apt install chafa
```

For supported Debian releases please lookup the
[package status page](https://tracker.debian.org/pkg/chafa).

## Installing from git

```
./autogen.sh
make
sudo make install
```

## Further reading

For tarball releases, online documentation, etc. see
[the official web pages](https://hpjansson.org/chafa/).
