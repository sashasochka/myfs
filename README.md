myfs
====

Simple and buggy user-space file system implementation

It's based on a simple file, in Unix-like file system you can mount /dev/* devices and it will potentially work with real device. 
It's very simple and has bugs.

The following features are supported:

  - mounting file as a device
  - file creation
  - hard links
  - sym links
  - directories
  - directory navigation (and relative paths (with bugs))
  - file and inode statistics
  
It uses the following layout:

  - device consits of blocks (512-bytes by default, easily changeble)
  - at the beginning device uses a few blocks as a bitmask for maintating other blocks (note: potential improvement here - use in-RAM caching)
  - each file has a descriptor (aka inode)
  - There are 3 types of files: directories, regular files, symlinks
  - directories contain an array of (hard) `Link`s to other files
  - regular files contain whatever you want
  - symlinks contain only a name of the file they're pointing to.
  

Known bugs:

  - bad handling of `..` and `.` in directory paths
  - not full support for big files.
  - some functions are just hanging if some incorrect data is passed. They should return some error code etc.
  

There are the following command aliases:

  - create <-> touch
  - read <-> cat
  - l <-> ls .
  - unlink <-> rm
  - filestat <-> stat

Example usage 


    >>> mount dev/hda
    File system mounted!
    >>> touch a
    File created
    >>> l
    a
    >>> write a
    Write some text to the file. When you want to finish writing write "end" in UPPERCASE symbols
    END
    Data successfully written
    >>> cat a
    Write some text to the file. When you want to finish writing write "end" in UPPERCASE symbols
    >>> stat a
    Type: regular
    Inode: 2
    Blocks uses(1): #4 
    Size: 93 bytes
    Number of (hard) links: 1
    >>> mkdir dir
    Dir created
    >>> cd dir
    cwd changed
    >>> write f
    /dir/f
    END
    File with name 'f' doesn't exist
    >>> create f
    File created
    >>> write f
    /dir/f
    END
    Data successfully written
    >>> ln f f_alias
    Link created
    >>> stat f
    Type: regular
    Inode: 6
    Blocks uses(1): #8 
    Size: 6 bytes
    Number of (hard) links: 2
    >>> stat f_alias
    Type: regular
    Inode: 6
    Blocks uses(1): #8 
    Size: 6 bytes
    Number of (hard) links: 2
    >>> stat .
    Type: directory
    Contains files: 2
    Inode: 5
    Blocks uses(1): #7 
    Size: 40 bytes
    Number of (hard) links: 1
    >>> cd ..
    cwd changed
    >>> ls
    .
    a
    dir
    >>> ls dir
    f
    f_alias
    >>> ln dir/f qw
    Link created
    >>> stat qw
    Type: regular
    Inode: 6
    Blocks uses(1): #8 
    Size: 6 bytes
    Number of (hard) links: 3
    >>> write qw
    One of three aliases
    END
    Data successfully written
    >>> cat dir/f
    One of three aliases
    >>> rm dir/f
    Hard link was removed
    >>> cat dir/f_alias
    One of three aliases
    >>> rm dir/f_alias
    Hard link was removed
    >>> stat f
    File with name 'f' doesn't exist
    >>> stat qw
    Type: regular
    Inode: 6
    Blocks uses(1): #8 
    Size: 20 bytes
    Number of (hard) links: 1
    >>> l
    a
    dir
    qw
    >>> symlink qs slink
    Target file doesn't exist
    >>> symlink qw slink
    Symlink created
    >>> cat slink
    One of three aliases
    >>> write slink
    The only file
    END
    Data successfully written
    >>> cat slink
    The only file
    >>> cat qw
    The only file
    >>> rm slink
    Hard link was removed
    >>> ls
    .
    a
    dir
    qw
    >>> rmdir dir
    Dir successfully removed
    >>> rm qw
    Hard link was removed
    >>> rm a
    Hard link was removed
    >>> umount
    File system unmounted!
