Virtual Audio Cable for MacOS X
===============================

See "Note on MacOS BigSur" below!
----------------------------------


Whats here
----------

What is deposited here is a folder containing the
Xcode project, so you should be able to re-compile
the driver on you Mac if you have Xcode. Furthermore,
the freshly compiled driver bundle (VACDevice.kext) is
also present. This is freshly compiled by Xcode on my
Intel-Mac running MacOS 10.15 "Catalina" and is the
same you can find somewhere deep in the Xcode folder.

For your convenience both the Xcode folder and the
driver are additionally available as ZIP files.

Disabling System Integrity Protection (SIP)
-------------------------------------------

Starting with MacOS 10.11 "El Capitan", Apple introduced
a very useful security feature that however disables
loading device drivers from untrusted persons (and I am
an untrusted person since I do not have the required
developer status). To disable SIP you have to boot
into the recovery screen (that means, hold option-R
while booting the Mac), open a terminal window and issue
the command

	csrutil disable

(you can re-infore SIP any time with the command "csrutil enable"
at the same place).

While it is not recommended to permanently switch off SIP, it is
the only way to use *this* driver. See "Alternatives" at the end
of this README for a commercial product that can be used without
disabling SIP.

Howto install
-------------

If you do not want to recompile the driver, download
the VACDevice.kext.zip file and un-compress it on your
desktop. Then, open a terminal window and issue the commands

	cd $HOME/Desktop
	sudo chown -R root:wheel VACDevice.kext
	sudo kextload VACDevice.kext

If you have the "Audio Midi Setup" window open, you should see
that two additional sound devices suddenly appear. If you want
to make this permanent, drag the VACDevice.kext into the
/Library/Extensions folder.


About the code
--------------

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
---------------

This code has been compiled unter MacOS 10.15 "Catalina" but has also
been tested in Intel Macs running "BigSur". Loading drivers is even
more restricted in BigSur:
After installing a kernel extension in MacOS BigSur, you must "allow" this
once (e.g. after rebooting) in the "Security" panel within the system preferences.
When you open it, the "Agree" button is already prepeared.

I cannot guarantee that the driver works for older MacOS versions
since I cannot test it, but you should be able to compile it on
fairly old MacOS versions. In fact, the code uses many features
already marked "deprecated" in MacOS 10.10, so it sooner or later
reaches its end-of-life.


Compilation on MacOS "BigSur"
------------------------------

Some users have reported that compilation fails after upgrade to BigSur.

What works for me is:
- delete XCode app from your Applications folder
- get and install XCode version 12.1, (you can get older XCode versions
  from Apple's Developer site)

After installing a kernel extension in MacOS BigSur, you must "allow" this
once (e.g. after rebooting) in the "Security" panel within the system preferences.
When you open it, the "Agree" button is already prepeared.

Alternatives
============

I  recently came across a loopback driver which you can buy, and which should
not require to disable SIP. Just search the internet for the key words

loopback rogue amoeba

and you will find a product which you can buy for about USD 100 (you
can evaluate before you pay). I can tell that it works because I
have bought it for use with my Apple M1 MacBookAir, where I have not even
tried to use my own driver.
