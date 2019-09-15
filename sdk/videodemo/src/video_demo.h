/************************************************************************/
/*                                                                      */
/*  video_demo.h -- Nexys Video HDMI demonstration                      */
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

#ifndef VIDEO_DEMO_H_
#define VIDEO_DEMO_H_

/* ------------------------------------------------------------ */
/*                  Include File Definitions                    */
/* ------------------------------------------------------------ */

#include "xil_types.h"


// schlto 30.06.2017
#include "_HMod/Nexys-Video-HDMIHMod.h"


/* ------------------------------------------------------------ */
/*                  Miscellaneous Declarations                  */
/* ------------------------------------------------------------ */

#define DEMO_PATTERN_0 0
#define DEMO_PATTERN_1 1

#define DEMO_MAX_FRAME (1920*1080*3)
#define DEMO_STRIDE (1920 * 3)

/*
 * Configure the Video capture driver to start streaming on signal
 * detection
 */
#define DEMO_START_ON_DET 1


// schlto 30.06.2017

bool enable_HMod;
bool HMod_inited;

u32 HMod_CPF;

u32   HMod_order;
float HMod_scale;
float HMod_radius;
u32   HMod_mode_i;
u32   HMod_mode_d;


/* ------------------------------------------------------------ */
/*                  Procedure Declarations                      */
/* ------------------------------------------------------------ */

void DemoInitialize();
void DemoRun();
void DemoPrintMenu();


// schlto 30.06.2017
void HMod_set_order();


void DemoChangeRes();
void DemoCRMenu();
void DemoInvertFrame(u8 *srcFrame, u8 *destFrame, u32 width, u32 height, u32 stride);
void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int pattern);
void DemoScaleFrame(u8 *srcFrame, u8 *destFrame, u32 srcWidth, u32 srcHeight, u32 destWidth, u32 destHeight, u32 stride);
void DemoISR(void *callBackRef, void *pVideo);

/* ------------------------------------------------------------ */

/************************************************************************/

#endif /* VIDEO_DEMO_H_ */

