[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/xfce/tumbler/-/blob/master/COPYING)

# tumbler


Tumbler is a D-Bus service for applications to request thumbnails for
various URI schemes and MIME types. It is an implementations of the 
thumbnail management D-Bud specification as described [here](https://wiki.gnome.org/DraftSpecs/ThumbnailerSpec)
Tumbler provides plugin interfaces for extending the URI schemes and MIME types
for which thumbnails can be generated as well as for replacing the storage
backend that is used to store the thumbnails on disk.

This fork adds thumbnail support for webp files, based of off this [forum thread](https://forum.xfce.org/viewtopic.php?id=8951)
and this [patch file](https://bugzilla.xfce.org/attachment.cgi?id=6641). The updated patch file made 
against the latest commit (3e6d8502de745a4e36c715779f71520a12607335) is given [here](https://gist.github.com/ru2saig/574f61db6bf18c73530cab39c58ccf87).

----

### Homepage

[Tumbler documentation](https://docs.xfce.org/xfce/tumbler/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/xfce/tumbler/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Tumbler source code](https://gitlab.xfce.org/xfce/tumbler)

### Download a Release Tarball

[Tumbler archive](https://archive.xfce.org/src/xfce/tumbler)
    or
[Tumbler tags](https://gitlab.xfce.org/xfce/tumbler/-/tags)

### Installation

From source: 

    % cd tumbler
    % ./autogen.sh
    % make
    % make install

From release tarball:

    % tar xf tumbler-<version>.tar.bz2
    % cd tumbler-<version>
    % ./configure
    % make
    % make install

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/xfce/tumbler/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

