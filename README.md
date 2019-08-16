Virtual Audio Cable for MacOS X
===============================


This code is derived from the sample code "MyAudioEngine"
in the book

O. H. Halvorsen and D. Clarke,
"iOS and Mac OS Kernel Programming",
Apress, 2011

(sample code available on GitHub).

It implements two virtual audio cables for the MacOS, 
named SDR-RX and SDR-TX. The purpose is to "connect"
an SDR application with digimode programs such as
fldigi or wsjt-x. To do so,

select "SDR-RX" as the RX output device ("Headphone")
and "SDR-TX" as the TX input device ("Microphone") in
the SDR application, and select "SDR-RX" for input and
"SDR-TX" for output in fldigi or wsjt-x.

The supported sample rates are 16000, 44100, and 48000 Hz,
each device provides two channels (stereo). All this
can easily be changed in the source code. For example,
it should be clear how to change the two lines

    if (!createAudioEngine("SDR-RX")) goto done;
    if (!createAudioEngine("SDR-TX")) goto done;

as to give more than two cables, possibly with other names.
It is also clear how to extend the code

            rate.whole = 16000;
            audioStream->addAvailableFormat(&format, &rate, &rate);
            rate.whole = 44100;
            audioStream->addAvailableFormat(&format, &rate, &rate);
            rate.whole = 48000;
            audioStream->addAvailableFormat(&format, &rate, &rate);

to provide more sample rates. As to the number of channels, it is
determined by the line

#define NumChan         2

and can easily be changed, but only for all cables simultaneously.

When comparing the driver with the sample code, you will notice that
I removed all controls such as Volume control. Well, this is a cable,
and cables just produce at the output what has been put in at the
input.

Mac OS Versions
===============


In this XCode project, I chose the "deployment" version as MacOS 10.9,
so it should run on every version therefrom (until IOKits are removed
by Apple, the seem to be deprecated now). To get rid of some silly warnings,
I forced XCode to use the old 10.9 SDK by the "Other Linker Flags" option
within the "Build settings" panel.
If you do not have the 10.9 SDK within your XCode app, simply delete this
option, it reads

OTHER_LDFLAGS = "-isysroot$(DEVELOPER_SDK_DIR)/MacOSX$(MACOSX_DEPLOYMENT_TARGET).sdk"

Install IOKit Drivers
=====================

IOKit drivers are either installed permanently by putting them into /Library/Extensions,
are activated and deactivated within the running system using kextload / kextunload.
However, you have to be aware of two points:

a) the driver must be owned by root. So, if you have moved the product named
VACDevice.kext to some place, you have to open a terminal window,
su to root, go to place where VACDevice.kext resides and issue the commands

	chown -R root:wheel VACDevice.kext
	chmod -R o-w VACDevice.kext

b) MacOS with System Integrity Protection (SIP) enabled does not allow unsigned 
drivers. I have an AppleID, and therefore I can sign the kext with a
MacDeveloper certificate, but this is not enough to get a kext loaded.
To get a Developer ID valid for kext signing, I need a "real" team with
a team agent etc., this is too heavy for me.

So for myself, I chose to dis-able  SIP temporarily when using the driver.
For this, you have to re-boot while holding Option-R, and when the system
shows the recovery screen, open a terminal window and type in 

csrutil disable

and reboot. Re-enabling SIP goes the same way, but type in "csrutil enable"
this time. I would not recommend to dis-able SIP permanently for security
reasons.

Alternatives
============

It does not feel OK that this only works with SIP disabled. I have recently
came across a loopback driver which you can buy. Just search the internet
for the key workds

loopback rogue amoeba

and you will find a product which you can buy for about 100 Dollars.
