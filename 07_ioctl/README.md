# ioctl

The ioctl() function manipulates the underlying device parameters of special
files. Most devices can perform operations beyond simple data transfers; user
space must often be able to request, for example, that the device lock its door,
eject its media, report error information, change a baud rate, or self destruct.
These operations are usually supported via the ioctl method.

