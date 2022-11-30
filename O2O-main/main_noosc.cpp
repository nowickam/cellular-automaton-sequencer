/*
 ____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\

The platform for ultra-low latency audio and sensor processing

http://bela.io

A project of the Augmented Instruments Laboratory within the
Centre for Digital Music at Queen Mary University of London.
http://www.eecs.qmul.ac.uk/~andrewm

(c) 2016 Augmented Instruments Laboratory: Andrew McPherson,
  Astrid Bin, Liam Donovan, Christian Heinrichs, Robert Jack,
  Giulio Moro, Laurel Pardue, Victor Zappi. All rights reserved.

The Bela software is distributed under the GNU Lesser General Public License
(LGPL 3.0), available here: https://www.gnu.org/licenses/lgpl-3.0.txt
*/
/**
\example Communication/oled-screen/main.cpp

Working with OLED Screens and OSC
---------------------------------

This example shows how to interface with an OLED screen.

We use the u8g2 library for drawing on the screen.

This program is an OSC receiver which listens for OSC messages which can be sent
from another programmes on your host computer or from your Bela project running
on the board.

This example comes with a Pure Data patch which can be run on your host computer
for testing the screen. Download local.pd from the project files. This patch
sends a few different OSC messages which are interpreted by the cpp code which
then draws on the screen. We will now explain what each of the osc messages does.

Select the correct constructor for your screen (info to be updated). When you run
this project the first thing you will see is a Bela logo!

The important part of this project happens in the parseMessage() function which
we'll talk through now.

/osc-test

This is just to test whether the OSC link with Bela is functioning. When this message
is received you will see "OSC TEST SUCCESS!" printed on the screen. In the PD patch
click the bang on the left hand side. If you don't see anything change on the screen
then try clicking the `connect bela.local 7562` again to reestablish the connection.
Note that you will have to do this every time the Bela programme restarts.

/number

This prints a number to the screen which is received as a float. Every time you click the bang
above this message a random number will be generated and displayed on the screen.

/display-text

This draws text on the screen using UTF8.

/parameters

This simulates the visualisation of 3 parameters which are generated as ramps in
the PD patch. They are sent over as floats and then used to change the width of
three filled boxes. We also write to the screen for the parameter labelling directly
from the cpp file.

/lfos

This is similar to the /parameters but with a different animation and a different
means of taking a snapshot of an audio signal (see the PD Patch). In this case the
value from the oscillator is used to scale the size on an ellipse.

/waveform

This is most flexible message receiver. It will draw any number of floats (up to
maximum number of pixels of your screen) that are sent after /waveform. In the PD
patch we have two examples of this. The first sends 5 floats which are displayed
as 5 bars. No that the range of the screen is scaled to 0.0 to 1.0.

The second example stores the values of an oscillator into an array of 128 values
(the number of pixels on our OLED screen, update the array size in the PD patch if you
have a different sized screen). These 128 floats (between 0.0 and 1.0) are sent ten
times a second.
*/

#include <signal.h>
#include <libraries/OscReceiver/OscReceiver.h>
#include <unistd.h>
#include "u8g2/U8g2LinuxI2C.h"
#include <vector>
#include <algorithm>

const unsigned int gI2cBus = 1;

// #define I2C_MUX // allow I2C multiplexing to select different target displays
struct Display
{
	U8G2 d;
	int mux;
};
std::vector<Display> gDisplays = {
	// use `-1` as the last value to indicate that the display is not behind a mux, or a number between 0 and 7 for its muxed channel number
	{U8G2_SH1106_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, gI2cBus, 0x3c), -1},
	// add more displays / addresses here
};

unsigned int gActiveTarget = 0;
// const int gLocalPort = 7562; //port for incoming OSC messages

#ifdef I2C_MUX
#include "TCA9548A.h"
const unsigned int gMuxAddress = 0x70;
TCA9548A gTca;
#endif // I2C_MUX

/// Determines how to select which display a message is targeted to:
typedef enum
{
	kTargetSingle,	 ///< Single target (one display).
	kTargetEach,	 ///< The first argument to each message is an index corresponding to the target display
	kTargetStateful, ///< Send a message to /target <float> to select which is the active display that all subsequent messages will be sent to
} TargetMode;

TargetMode gTargetMode = kTargetSingle; // can be changed with /targetMode
OscReceiver oscReceiver;
int gStop = 0;

// Handle Ctrl-C by requesting that the audio rendering stop
void interrupt_handler(int var)
{
	gStop = true;
}

static void switchTarget(int target)
{
#ifdef I2C_MUX
	if (target >= gDisplays.size())
		return;
	U8G2 &u8g2 = gDisplays[target].d;
	int mux = gDisplays[target].mux;
	static int oldMux = -1;
	if (oldMux != mux)
		gTca.select(mux);
	oldTarget = target;
#endif // I2C_MUX
	gActiveTarget = target;
}

int main(int main_argc, char *main_argv[])
{
	if (0 == gDisplays.size())
	{
		fprintf(stderr, "No displays in gDisplays\n");
		return 1;
	}
#ifdef I2C_MUX
	if (gTca.initI2C_RW(gI2cBus, gMuxAddress, -1) || gTca.select(-1))
	{
		fprintf(stderr, "Unable to initialise the TCA9548A multiplexer. Are the address and bus correct?\n");
		return 1;
	}
#endif // I2C_MUX
	for (unsigned int n = 0; n < gDisplays.size(); ++n)
	{
		switchTarget(n);
		U8G2 &u8g2 = gDisplays[gActiveTarget].d;
#ifndef I2C_MUX
		int mux = gDisplays[gActiveTarget].mux;
		if (-1 != mux)
		{
			fprintf(stderr, "Display %u requires mux %d but I2C_MUX is disabled\n", n, mux);
			return 1;
		}
#endif // I2C_MUX
		u8g2.initDisplay();
		u8g2.setPowerSave(0);
		u8g2.clearBuffer();
		u8g2.setFont(u8g2_font_4x6_tf);
		u8g2.setFontRefHeightText();
		u8g2.setFontPosTop();
		u8g2.drawPixel(0, 0);
		// u8g2.drawStr(0, 0, " ____  _____ _        _");
		// u8g2.drawStr(0, 7, "| __ )| ____| |      / \\");
		// u8g2.drawStr(0, 14, "|  _ \\|  _| | |     / _ \\");
		// u8g2.drawStr(0, 21, "| |_) | |___| |___ / ___ \\");
		// u8g2.drawStr(0, 28, "|____/|_____|_____/_/   \\_\\");
		if (gDisplays.size() > 1)
		{
			std::string targetString = "Target ID: " + std::to_string(n);
			u8g2.drawStr(0, 50, targetString.c_str());
		}
		u8g2.sendBuffer();
	}
	// Set up interrupt handler to catch Control-C and SIGTERM
	signal(SIGINT, interrupt_handler);
	signal(SIGTERM, interrupt_handler);
	// OSC
	// oscReceiver.setup(gLocalPort, parseMessage);
	while (!gStop)
	{
		usleep(100000);
	}
	return 0;
}
