- Let people know of a pending release;

  mail to info-vcdimager@gnu.org
  no major changes before release, please

- test on lots of platforms; FSF's compile farm, for example

- "make distcheck" should work.

- From `Changelog` add `NEWS`. Update date of release.

- Remove "git from configure.ac's release name. E.g.

```
    define(RELEASE_NUM, 0rc1)
    define(LIBVCD_VERSION_STR, 2.0.$1git)
                                   ^^^
```

- Make sure sources are current and checked in:

    git pull
	git commit .

- Rebuild from scratch

```
  ./autogen.sh && make && make check`
   export VCDIMAGER_VERSION=2.0.0 # adjust
  git commit -m"Get ready for release ${VCDIMAGER_VERSION}" .
 `make ChangeLog
``

- Tag release in git:

```
  git push
  git tag release-${VCDIMAGER_VERSION}
  git push --tags
```

- `make distcheck` one more time

- Get onto ftp.gnu.org. I use my perl program

   gnupload from the automake distribution.
   locate gnupload
   ~/bin/gnupload --to ftp.gnu.org:vcdimager vcdimager-${VCDIMAGER_VERSION}.tar.*  # (Use "is" password)

- Make sure sources are current and checked in:
    git pull
    git commit .

- Send mail to info-vcdimager@gnu.org and libcdio-devel@gnu.org

  Again, The NEWS file is your friend.

- copy doxygen html to web pages

- Update www.vcdimager.org
  Copy tar.gz, NEWS, and ChangeLog files
  Update NEWS flashes

- Bump version in configure.ac and add "git. See places above in
  removal
