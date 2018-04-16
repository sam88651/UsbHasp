# UsbHasp
Research of usb hasp protection in Linux environment
There are more or less successful projects studying Usb Hasp in Windows. There are even various emulators demonstrating software upproach of applications protection.

This project is about Usb hasp emulator on Linux. In particular on Debian based platforms.

Emulator consists of several parts:
- usb-vhci driver - http://sourceforge.net/projects/usb-vhci/, http://sourceforge.net/p/usb-vhci/wiki/Home/. Original usb-vhci requires some modifications to work in modern kernels. I won't provide any patch, whoever is interested and qualified enough can fix sources.
- usb hasp emulator daemon - this project.
- Sentinel hasp drivers - easy to find and download
- and optional hasmlm daemon for network keys support - same thing, easy to google and get

I'd like to say that my project is based on many works and pieces of code developed for Windows of various versions. I thank all of you who shared their code. Now it's my turn.
Dependencies: usb_vhci library, jasson library.

