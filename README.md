### mandr
A tldr-pages client for UNIX-like OS.

### Usage
```
mandr - A CLI TL;DR pages viewer
Usage:
[--lang] -- Select TL;DR page language
[--platform] -- Select platform specific TL;DR page
[--cmd] -- Command name
[--symlink] -- create executable symlink in /bin
[--update] -- update all TL;DR pages
[--update-all] -- sync and update all TL;DR pages
[--help] -- Shows this help menu
```

#### Examples
1. `mandr --platform linux --cmd tar`
2. `mandr --platform windows --cmd tree`
3. `mandr --platform any --cmd zfs`
4. `mandr --platform android --lang ru --cmd logcat`

### Notes
If both platforms share the same command, for example in Linux there's `dmesg` command, and in SunOS, there's `dmesg` too, that means if you specify platform as `any` it'd only show the first match. It's always recommended to specify your working platform to avoid any confusion. 
