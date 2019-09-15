/************************************************************************/
/*                                                                      */
/*  video_demo.c -- Nexys Video HDMI demonstration                      */
/*                                                                      */
/************************************************************************/
/*  Author: Sam Bobrowicz                                               */
/*  Copyright 2015, Digilent Inc.                                       */
/************************************************************************/
/*  Module Description:                                                 */
/*                                                                      */
/*      This file contains code for running a demonstration of the      */
/*      Video input and output capabilities on the Nexys Video. It is   */
/*      a good example of how to properly use the display_ctrl and      */
/*      video_capture drivers.                                          */
/*                                                                      */
/*                                                                      */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*      11/25/2015(SamB): Created                                       */
/*      03/31/2017(ArtVVB): Updated sleep functions for 2016.4          */
/*                                                                      */
/************************************************************************/

/* ------------------------------------------------------------ */
/*                  Include File Definitions                    */
/* ------------------------------------------------------------ */

#include "video_demo.h"
#include "video_capture/video_capture.h"
#include "display_ctrl/display_ctrl.h"
#include "intc/intc.h"
#include <stdio.h>
#include "xuartlite_l.h"
//#include "xuartps.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "sleep.h"


// schlto 30.06.2017

#include "_HMod/Nexys-Video-HDMIHMod.h"

#include "xtmrctr_l.h"


/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
#define VID_VTC_ID XPAR_VTC_1_DEVICE_ID
#define VID_GPIO_ID XPAR_AXI_GPIO_VIDEO_DEVICE_ID
#define VID_VTC_IRPT_ID XPAR_INTC_0_VTC_1_VEC_ID
#define VID_GPIO_IRPT_ID XPAR_INTC_0_GPIO_0_VEC_ID
#define SCU_TIMER_ID XPAR_AXI_TIMER_0_DEVICE_ID
#define UART_BASEADDR XPAR_UARTLITE_0_BASEADDR


// schlto 30.06.2017

// < 5 * 10^6 CPF
#define CLEAR_FB(FB) ( \
	memset(FB, 0, 3 * videoCapt.timing.HActiveVideo *  \
		videoCapt.timing.VActiveVideo * sizeof(FB[0])) \
)


/* ------------------------------------------------------------ */
/*                  Global Variables                            */
/* ------------------------------------------------------------ */

/*
 * Display and Video Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma vdma;
VideoCapture videoCapt;
INTC intc;
char fRefresh; //flag used to trigger a refresh of the Menu on video detect

/*
 * Framebuffers for video data
 */
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];
u8 *pFrames[DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers

/*
 * Interrupt vector table
 */
const ivt_t ivt[] = {
	videoGpioIvt(VID_GPIO_IRPT_ID, &videoCapt),
	videoVtcIvt(VID_VTC_IRPT_ID, &(videoCapt.vtc))
};


// schlto 30.06.2017

bool enable_HMod = false;
bool HMod_inited = false;

u32 HMod_CPF = 0;

u32   HMod_order  = 5;
float HMod_scale  = 1.0f;
float HMod_radius = 1.0f;
u32   HMod_mode_i = 0;
u32   HMod_mode_d = 0;


/* ------------------------------------------------------------ */
/*                  Procedure Definitions                       */
/* ------------------------------------------------------------ */

int main(void)
{
	Xil_ICacheEnable();
	Xil_DCacheEnable();

	DemoInitialize();

	DemoRun();

	return 0;
}


void DemoInitialize()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;
	int i;

	/*
	 * Initialize an array of pointers to the 3 frame buffers
	 */
	for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
	{
		pFrames[i] = frameBuf[i];
	}

	/*
	 * Initialize VDMA driver
	 */
	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
		return;
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Interrupt controller and start it.
	 */
	Status = fnInitInterruptController(&intc);
	if(Status != XST_SUCCESS) {
		xil_printf("Error initializing interrupts");
		return;
	}
	fnEnableInterrupts(&intc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));

	/*
	 * Initialize the Video Capture device
	 */
	Status = VideoInitialize(&videoCapt, &intc, &vdma, VID_GPIO_ID, VID_VTC_ID, VID_VTC_IRPT_ID, pFrames, DEMO_STRIDE, DEMO_START_ON_DET);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Video Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Set the Video Detect callback to trigger the menu to reset, displaying the new detected resolution
	 */
	VideoSetCallback(&videoCapt, DemoISR, &fRefresh);

	DemoPrintTest(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, DEMO_PATTERN_1);

	return;
}

void DemoRun()
{
	int nextFrame = 0;
	char userInput = 0;
	u32 locked;
	XGpio *GpioPtr = &videoCapt.gpio;

	/* Flush UART FIFO */
	while (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
	{
		XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
	}
	while (userInput != 'q')
	{


		// schlto 30.06.2017
		if(enable_HMod && HMod_inited && nextFrame/* == 2*/) {
			XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID, 0);
			XTmrCtr_SetLoadReg(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID, 0);
			XTmrCtr_LoadTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID);
			u32 cs = XTmrCtr_GetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID);
			XTmrCtr_SetControlStatusReg(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID, cs & ~XTC_CSR_LOAD_MASK);


			VideoStop(&videoCapt);
			HMod_CPF = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID);
			XTmrCtr_Enable(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID);

			NexysVideoHDMIHMod(
				pFrames[videoCapt.curFrame], pFrames[nextFrame],
				videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, dispCtrl.vMode.width, dispCtrl.vMode.height,
				HMod_order, HMod_scale, HMod_radius, HMod_mode_i, HMod_mode_d);

			Xil_DCacheFlushRange((unsigned int)pFrames[nextFrame], DEMO_MAX_FRAME);

			XTmrCtr_Disable(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID);
			HMod_CPF = XTmrCtr_GetTimerCounterReg(XPAR_AXI_TIMER_0_BASEADDR, XPAR_AXI_TIMER_0_DEVICE_ID) - HMod_CPF;
			VideoStart(&videoCapt);
		}


		fRefresh = 0;
		DemoPrintMenu();

		/* Wait for data on UART */
		while (XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh)
		{}

		/* Store the first character in the UART receive FIFO and echo it */
		if (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
		{
			userInput = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
			xil_printf("%c", userInput);
		}
		else  //Refresh triggered by video detect interrupt
		{
			userInput = 'r';
		}

		switch (userInput)
		{


			// schlto 30.06.2017

			case 'p':
				if(!HMod_inited) {
					xil_printf("\x1B[H");
					xil_printf("\x1B[2J");
					xil_printf("Initializing HMod...");

					VideoStop(&videoCapt);

					NexysVideoHDMIHMod_init(
						dispCtrl.vMode.width, dispCtrl.vMode.height,
						HMod_order, HMod_scale, HMod_radius);

					VideoStart(&videoCapt);

					HMod_inited = true;
				}
				break;
			case 'P':
				if(HMod_inited) {
					xil_printf("\x1B[H");
					xil_printf("\x1B[2J");
					xil_printf("Freeing Memory (Precalculations)...");

					VideoStop(&videoCapt);
					NexysVideoHDMIHMod_free();
					VideoStart(&videoCapt);

					HMod_inited = false;
					HMod_CPF    = 0;
				}
				break;

			case 'h':
				if(!enable_HMod) {
					nextFrame = 2;
					DisplayChangeFrame(&dispCtrl, nextFrame);

					enable_HMod = true;
				}
				break;
			case 'H':
				if(enable_HMod) {
					nextFrame = 0;
					DisplayChangeFrame(&dispCtrl, nextFrame);

					enable_HMod = false;
					HMod_CPF    = 0;
				}
				break;

			case 'o':
				HMod_set_order();
				break;


			case 'i':
				CLEAR_FB(pFrames[nextFrame]);

				if(HMod_mode_i < 3) {
					HMod_mode_i++;
				} else {
					HMod_mode_i = 0;
				}

				break;


			case 'd':
				if(HMod_mode_d) {
					CLEAR_FB(pFrames[nextFrame]);

					HMod_mode_d = 0;
				}
				break;
			case 'D':
				if(!HMod_mode_d) {
					CLEAR_FB(pFrames[nextFrame]);

					HMod_mode_d = 1;
				}
				break;


		case '1':
			DemoChangeRes();
			break;
		case '2':
			nextFrame = dispCtrl.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '3':
			DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_0);
			break;
		case '4':
			DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_1);
			break;
		case '5':
			if (videoCapt.state == VIDEO_STREAMING)
				VideoStop(&videoCapt);
			else
				VideoStart(&videoCapt);
			break;
		case '6':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			VideoChangeFrame(&videoCapt, nextFrame);
			break;
		case '7':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			VideoStop(&videoCapt);
			DemoInvertFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '8':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES)
			{
				nextFrame = 0;
			}
			VideoStop(&videoCapt);
			DemoScaleFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case 'q':
			break;
		case 'r':
			locked = XGpio_DiscreteRead(GpioPtr, 2);
			xil_printf("%d", locked);
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			usleep(50000);
		}
	}

	return;
}

void DemoPrintMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal


	// schlto 30.06.2017
	xil_printf("**************************************************\n\r");
	xil_printf("*   Hexagonal Image Processing Framework CHIP    *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("* enable_HMod = %u (inited = %u):                  *\n\r", \
	              enable_HMod,      HMod_inited);
	xil_printf("*  order = %u, mode_i = %u, mode_d = %u             *\n\r", \
	               HMod_order, HMod_mode_i, HMod_mode_d);
	xil_printf("**************************************************\n\r");
	xil_printf("* CPF: %41u *\n\r", HMod_CPF);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("p/P - Init. HMod / free Memory (Precalculations)  \n\r");
	xil_printf("h/H - Enable/disable HMod                         \n\r");
	xil_printf("o   - Set Hexarray Order (Menu)                   \n\r");
	xil_printf("i   - Set Interpolation Mode:                     \n\r");
	xil_printf("       BL / BC / Lanczos / B-Splines (B_3)        \n\r");
	xil_printf("d/D - Set Display Mode: hex/sq                    \n\r");
	xil_printf("\n\r");
	xil_printf("\n\r");


	xil_printf("**************************************************\n\r");
	xil_printf("*             Nexys Video HDMI Demo              *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*Display Resolution: %28s*\n\r", dispCtrl.vMode.label);
	printf("*Display Pixel Clock Freq. (MHz): %15.3f*\n\r", dispCtrl.pxlFreq);
	xil_printf("*Display Frame Index: %27d*\n\r", dispCtrl.curFrame);
	if (videoCapt.state == VIDEO_DISCONNECTED) xil_printf("*Video Capture Resolution: %22s*\n\r", "!HDMI UNPLUGGED!");
	else xil_printf("*Video Capture Resolution: %17dx%-4d*\n\r", videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo);
	xil_printf("*Video Frame Index: %29d*\n\r", videoCapt.curFrame);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - Change Display Resolution\n\r");
	xil_printf("2 - Change Display Framebuffer Index\n\r");
	xil_printf("3 - Print Blended Test Pattern to Display Framebuffer\n\r");
	xil_printf("4 - Print Color Bar Test Pattern to Display Framebuffer\n\r");
	xil_printf("5 - Start/Stop Video stream into Video Framebuffer\n\r");
	xil_printf("6 - Change Video Framebuffer Index\n\r");
	xil_printf("7 - Grab Video Frame and invert colors\n\r");
	xil_printf("8 - Grab Video Frame and scale to Display resolution\n\r");
	xil_printf("q - Quit\n\r");
	xil_printf("\n\r");
	xil_printf("\n\r");
	xil_printf("Enter a selection: ");
}


// schlto 30.06.2017
void HMod_set_order() {
	bool order_set = false;
	char input     = 0; // XUartLite_ReadReg

	while(!XUartLite_IsReceiveEmpty(UART_BASEADDR))
		XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);

	while(!order_set) {
		xil_printf("\x1B[H");
		xil_printf("\x1B[2J");
		xil_printf("**************************************************\n\r");
		xil_printf("*   Hexagonal Image Processing Framework CHIP    *\n\r");
		xil_printf("**************************************************\n\r");
		xil_printf("* enable_HMod = %u (inited = %u):                  *\n\r", \
		              enable_HMod,      HMod_inited);
		xil_printf("*  order = %u, mode_i = %u, mode_d = %u             *\n\r", \
		               HMod_order, HMod_mode_i, HMod_mode_d);
		xil_printf("**************************************************\n\r");
		xil_printf("* CPF: %41u *\n\r", HMod_CPF);
		xil_printf("**************************************************\n\r");
		xil_printf("\n\r");
		xil_printf("3-6 - Hexarray Order (Size 7^3-7^6)               \n\r");
		xil_printf("q   - Quit (don't change Order)                   \n\r");
		xil_printf("\n\r");
		xil_printf("\n\r");
		xil_printf("Enter new Hexarray Order: ");


		while(XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh);
		input = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
		xil_printf("%c", input);

		if(input > '2' && input < '7') {
			HMod_order = (u32)(input - '0');

			order_set = true;
		} else if(input == 'q') {
			order_set = true;
		} else {
			xil_printf("\n\rInvalid Selection");
			usleep(50000);
		}
	}
}


void DemoChangeRes()
{
	int fResSet = 0;
	int status;
	char userInput = 0;

	/* Flush UART FIFO */
	while (!XUartLite_IsReceiveEmpty(UART_BASEADDR))
		{
			XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
		}

	while (!fResSet)
	{
		DemoCRMenu();

		/* Wait for data on UART */
		while (XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh)
		{}

		/* Store the first character in the UART recieve FIFO and echo it */

		userInput = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
		xil_printf("%c", userInput);
		status = XST_SUCCESS;
		switch (userInput)
		{
		case '1':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_640x480);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '2':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_800x600);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '3':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1280x720);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '4':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1280x1024);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case '5':
			status = DisplayStop(&dispCtrl);
			DisplaySetMode(&dispCtrl, &VMODE_1920x1080);
			DisplayStart(&dispCtrl);
			fResSet = 1;
			break;
		case 'q':
			fResSet = 1;
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			usleep(50000);
		}
		if (status == XST_DMA_ERROR)
		{
			xil_printf("\n\rWARNING: AXI VDMA Error detected and cleared\n\r");
		}
	}
}

void DemoCRMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal


	// schlto 30.06.2017
	xil_printf("**************************************************\n\r");
	xil_printf("*   Hexagonal Image Processing Framework CHIP    *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("* enable_HMod = %u (inited = %u):                  *\n\r", \
	              enable_HMod,      HMod_inited);
	xil_printf("*  order = %u, mode_i = %u, mode_d = %u             *\n\r", \
	               HMod_order, HMod_mode_i, HMod_mode_d);
	xil_printf("**************************************************\n\r");
	xil_printf("* CPF: %41u *\n\r", HMod_CPF);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("\n\r");


	xil_printf("**************************************************\n\r");
	xil_printf("*             Nexys Video HDMI Demo              *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*Current Resolution: %28s*\n\r", dispCtrl.vMode.label);
	printf("*Pixel Clock Freq. (MHz): %23.3f*\n\r", dispCtrl.pxlFreq);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - %s\n\r", VMODE_640x480.label);
	xil_printf("2 - %s\n\r", VMODE_800x600.label);
	xil_printf("3 - %s\n\r", VMODE_1280x720.label);
	xil_printf("4 - %s\n\r", VMODE_1280x1024.label);
	xil_printf("5 - %s\n\r", VMODE_1920x1080.label);
	xil_printf("q - Quit (don't change resolution)\n\r");
	xil_printf("\n\r");


	// schlto 30.06.2017
	xil_printf("\n\r");


	xil_printf("Select a new resolution: ");
}

void DemoInvertFrame(u8 *srcFrame, u8 *destFrame, u32 width, u32 height, u32 stride)
{
	u32 xcoi, ycoi;
	u32 lineStart = 0;
	for(ycoi = 0; ycoi < height; ycoi++)
	{
		for(xcoi = 0; xcoi < (width * 3); xcoi+=3)
		{
			destFrame[xcoi + lineStart] = ~srcFrame[xcoi + lineStart];         //Red
			destFrame[xcoi + lineStart + 1] = ~srcFrame[xcoi + lineStart + 1]; //Blue
			destFrame[xcoi + lineStart + 2] = ~srcFrame[xcoi + lineStart + 2]; //Green
		}
		lineStart += stride;
	}
	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);
}


/*
 * Bilinear interpolation algorithm. Assumes both frames have the same stride.
 */
void DemoScaleFrame(u8 *srcFrame, u8 *destFrame, u32 srcWidth, u32 srcHeight, u32 destWidth, u32 destHeight, u32 stride)
{
	float xInc, yInc; // Width/height of a destination frame pixel in the source frame coordinate system
	float xcoSrc, ycoSrc; // Location of the destination pixel being operated on in the source frame coordinate system
	float x1y1, x2y1, x1y2, x2y2; //Used to store the color data of the four nearest source pixels to the destination pixel
	int ix1y1, ix2y1, ix1y2, ix2y2; //indexes into the source frame for the four nearest source pixels to the destination pixel
	float xDist, yDist; //distances between destination pixel and x1y1 source pixels in source frame coordinate system

	int xcoDest, ycoDest; // Location of the destination pixel being operated on in the destination coordinate system
	int iy1; //Used to store the index of the first source pixel in the line with y1
	int iDest; //index of the pixel data in the destination frame being operated on

	int i;

	xInc = ((float) srcWidth - 1.0) / ((float) destWidth);
	yInc = ((float) srcHeight - 1.0) / ((float) destHeight);

	ycoSrc = 0.0;
	for (ycoDest = 0; ycoDest < destHeight; ycoDest++)
	{
		iy1 = ((int) ycoSrc) * stride;
		yDist = ycoSrc - ((float) ((int) ycoSrc));

		/*
		 * Save some cycles in the loop below by presetting the destination
		 * index to the first pixel in the current line
		 */
		iDest = ycoDest * stride;

		xcoSrc = 0.0;
		for (xcoDest = 0; xcoDest < destWidth; xcoDest++)
		{
			ix1y1 = iy1 + ((int) xcoSrc) * 3;
			ix2y1 = ix1y1 + 3;
			ix1y2 = ix1y1 + stride;
			ix2y2 = ix1y1 + stride + 3;

			xDist = xcoSrc - ((float) ((int) xcoSrc));

			/*
			 * For loop handles all three colors
			 */
			for (i = 0; i < 3; i++)
			{
				x1y1 = (float) srcFrame[ix1y1 + i];
				x2y1 = (float) srcFrame[ix2y1 + i];
				x1y2 = (float) srcFrame[ix1y2 + i];
				x2y2 = (float) srcFrame[ix2y2 + i];

				/*
				 * Bilinear interpolation function
				 */
				destFrame[iDest] = (u8) ((1.0-yDist)*((1.0-xDist)*x1y1+xDist*x2y1) + yDist*((1.0-xDist)*x1y2+xDist*x2y2));
				iDest++;
			}
			xcoSrc += xInc;
		}
		ycoSrc += yInc;
	}

	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);

	return;
}

void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int pattern)
{
	u32 xcoi, ycoi;
	u32 iPixelAddr;
	u8 wRed, wBlue, wGreen;
	u32 wCurrentInt;
	double fRed, fBlue, fGreen, fColor;
	u32 xLeft, xMid, xRight, xInt;
	u32 yMid, yInt;
	double xInc, yInc;


	switch (pattern)
	{
	case DEMO_PATTERN_0:

		xInt = width / 4; //Four intervals, each with width/4 pixels
		xLeft = xInt * 3;
		xMid = xInt * 2 * 3;
		xRight = xInt * 3 * 3;
		xInc = 256.0 / ((double) xInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		yInt = height / 2; //Two intervals, each with width/2 lines
		yMid = yInt;
		yInc = 256.0 / ((double) yInt); //256 color intensities are cycled through per interval (overflow must be caught when color=256.0)

		fBlue = 0.0;
		fRed = 256.0;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3)
		{
			/*
			 * Convert color intensities to integers < 256, and trim values >=256
			 */
			wRed = (fRed >= 256.0) ? 255 : ((u8) fRed);
			wBlue = (fBlue >= 256.0) ? 255 : ((u8) fBlue);
			iPixelAddr = xcoi;
			fGreen = 0.0;
			for(ycoi = 0; ycoi < height; ycoi++)
			{

				wGreen = (fGreen >= 256.0) ? 255 : ((u8) fGreen);
				frame[iPixelAddr] = wRed;
				frame[iPixelAddr + 1] = wBlue;
				frame[iPixelAddr + 2] = wGreen;
				if (ycoi < yMid)
				{
					fGreen += yInc;
				}
				else
				{
					fGreen -= yInc;
				}

				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			if (xcoi < xLeft)
			{
				fBlue = 0.0;
				fRed -= xInc;
			}
			else if (xcoi < xMid)
			{
				fBlue += xInc;
				fRed += xInc;
			}
			else if (xcoi < xRight)
			{
				fBlue -= xInc;
				fRed -= xInc;
			}
			else
			{
				fBlue += xInc;
				fRed = 0;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	case DEMO_PATTERN_1:

		xInt = width / 7; //Seven intervals, each with width/7 pixels
		xInc = 256.0 / ((double) xInt); //256 color intensities per interval. Notice that overflow is handled for this pattern.

		fColor = 0.0;
		wCurrentInt = 1;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3)
		{

			/*
			 * Just draw white in the last partial interval (when width is not divisible by 7)
			 */
			if (wCurrentInt > 7)
			{
				wRed = 255;
				wBlue = 255;
				wGreen = 255;
			}
			else
			{
				if (wCurrentInt & 0b001)
					wRed = (u8) fColor;
				else
					wRed = 0;

				if (wCurrentInt & 0b010)
					wBlue = (u8) fColor;
				else
					wBlue = 0;

				if (wCurrentInt & 0b100)
					wGreen = (u8) fColor;
				else
					wGreen = 0;
			}

			iPixelAddr = xcoi;

			for(ycoi = 0; ycoi < height; ycoi++)
			{
				frame[iPixelAddr] = wRed;
				frame[iPixelAddr + 1] = wBlue;
				frame[iPixelAddr + 2] = wGreen;
				/*
				 * This pattern is printed one vertical line at a time, so the address must be incremented
				 * by the stride instead of just 1.
				 */
				iPixelAddr += stride;
			}

			fColor += xInc;
			if (fColor >= 256.0)
			{
				fColor = 0.0;
				wCurrentInt++;
			}
		}
		/*
		 * Flush the framebuffer memory range to ensure changes are written to the
		 * actual memory, and therefore accessible by the VDMA.
		 */
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	default :
		xil_printf("Error: invalid pattern passed to DemoPrintTest");
	}
}

void DemoISR(void *callBackRef, void *pVideo)
{
	char *data = (char *) callBackRef;
	*data = 1; //set fRefresh to 1
}
