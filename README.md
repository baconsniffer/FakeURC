# FakeURC
Emulates the data interface of a General Dynamics URC200 radio.

A while back I was tasked with creating a front end to remotely control
a General Dynamics URC-200 radio. The interface runs on a Windows machine
and uses a fancy serial interconnect to remotely control the radio through
the RS-232 serial interface built into the radio.

During development I had some medical issues and required surgery. After
a period of recovery I was able to work again but did not have the physical
strength nor the ability to drive myself to and from work for some time.

Fortunately I was able to work on other projects but I bothered me that
I couldn't continue on the remote control interface.
Then I had a thought: I don't need the radio, just it's interface.
The protocol for communicating with the radio is well documented so why
couldn't I just make an emulator that spoke the protocol and responded
as the radio would. So I decided that in my after-hours time, I would
work on an emulator and maybe learn something new in the process.

Since the interface was relatively simple and the serial speed very slow
(1200 baud!) an Arduino microcontroller would be ideal since it has
a well supported eco-system that I was already familiar with and would
appear to the Windows computer as a serial port.

I coded a basic emulator that ran on a Uno R3 and it worked great. The only
drawback was that I had no way of being certain that the commands I sent
to the emulated radio were actually having an effect since there was no
way to examine the memory state of the Arduino while it was running.
Conversely, there was no easy way I could think of to allow modification to
the state of the radio (mic keyed, incoming transmission, squelch level
adjustment, etc). I thought about hooking up some switches and a potentiometer
or two but then I'd have a big, ugly breadboard and still not have the level
fidelity I wanted.

Fortunately I also had a Arduino Mega2560 with an big TFT LCD screen that
I had picked up on an Amazon deal and had stashed away thinking "This is too
good a deal to pass up and I will find some use for it one day". That
day had arrived.

The 380x420 LCD screen allows plenty of space to display the internal
state of the radio and I can see the effects of the commands sent to
it in ral time. The screen also has a touch-screen interface so that meant
that I could also make changes to the emulated state of the radio.

The result is this sketch for the Mega. It is not a perfect, cycle-accurate
emulation of the real URC-200. However, it is functional enough that it
appears to front-end controller software as it were the real thing.

I learned a lot developing this project both about low-level GUI interface and
design and compromises and limitations of emulation.

This project may have no practical application other than as a tool for
myself to allow me to continue work on a project that would otherwise not
have been possible. Anyone who finds this code and thinks it might be useful,
all I ask is that you not use it to make money nor try to sell it (even modified).
Use it to learn and to grow.
