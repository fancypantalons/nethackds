/*---------------------------------------------------------------------------------
	$Id: template.c,v 1.2 2005/09/07 20:06:06 wntrmute Exp $

	Simple ARM7 stub (sends RTC, TSC, and X/Y data to the ARM 9)

	$Log: template.c,v $
	Revision 1.2  2005/09/07 20:06:06  wntrmute
	updated for latest libnds changes
	
	Revision 1.8  2005/08/03 05:13:16  wntrmute
	corrected sound code


---------------------------------------------------------------------------------*/
#include <nds.h>

#include <nds/bios.h>
#include <nds/arm7/touch.h>
#include <nds/arm7/clock.h>

#include <dswifi7.h>

#include "ndsx_ledblink.h"

//---------------------------------------------------------------------------------
void startSound(int sampleRate, const void* data, u32 bytes, u8 channel, u8 vol,  u8 pan, u8 format) {
//---------------------------------------------------------------------------------
	SCHANNEL_TIMER(channel)  = SOUND_FREQ(sampleRate);
	SCHANNEL_SOURCE(channel) = (u32)data;
	SCHANNEL_LENGTH(channel) = bytes >> 2 ;
	SCHANNEL_CR(channel)     = SCHANNEL_ENABLE | SOUND_ONE_SHOT | SOUND_VOL(vol) | SOUND_PAN(pan) | (format==1?SOUND_8BIT:SOUND_16BIT);
}


//---------------------------------------------------------------------------------
s32 getFreeSoundChannel() {
//---------------------------------------------------------------------------------
	int i;
	for (i=0; i<16; i++) {
		if ( (SCHANNEL_CR(i) & SCHANNEL_ENABLE) == 0 ) return i;
	}
	return -1;
}

#define SAMPLE_COUNT 16

void sortTouchSamples(touchPosition *samples, int *indices, int sort_x)
{
  int i;
  int done = 0;

  /* Yeah, this is a bubble sort... live with it */
  /* We re-order, here, so we can take the median */

  while (! done) {
    done = 1;

    for (i = 0; i < SAMPLE_COUNT - 1; i++) {
      int swap = sort_x ? samples[indices[i]].x > samples[indices[i + 1]].x :
                          samples[indices[i]].y > samples[indices[i + 1]].y;

      if (swap) {
        int tmp = indices[i + 1];

        indices[i + 1] = indices[i];
        indices[i] = tmp;
        
        done = 0;
      }
    }
  }
}

touchPosition readSampledPosition() {
  touchPosition posData[SAMPLE_COUNT];
  int indices_by_x[SAMPLE_COUNT];
  int indices_by_y[SAMPLE_COUNT];
  int i;
  touchPosition ret;

  /* First, take our samples */

  for (i = 0; i < SAMPLE_COUNT; i++) {
    while (1) {
      posData[i] = touchReadXY();

      /* 
       * Yeah, this means you can't tap the far-right part of the screen...
       * luckily, I don't care.  And this way, we toss out outliers that
       * were throwing off the touchscreen handling code.
       */
      if (posData[i].px < 255) {
        break;
      }
    }

    indices_by_x[i] = i;
    indices_by_y[i] = i;
  }

  sortTouchSamples(posData, indices_by_x, 1);
  sortTouchSamples(posData, indices_by_y, 0);

  /* And now get the median values for x and y */

  ret.x = posData[indices_by_x[SAMPLE_COUNT / 2]].x;
  ret.px = posData[indices_by_x[SAMPLE_COUNT / 2]].px;

  ret.y = posData[indices_by_y[SAMPLE_COUNT / 2]].y;
  ret.py = posData[indices_by_y[SAMPLE_COUNT / 2]].py;

  return ret;
}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	uint16 but=0, x=0, y=0, xpx=0, ypx=0, z1=0, z2=0, batt=0, aux=0;
	int t1=0, t2=0;
	uint32 temp=0;
	// uint8 ct[sizeof(IPC->time.curtime)];
	u32 i;

	// Read the touch screen

	but = REG_KEYXY;

	if (!(but & (1<<6))) {
                touchPosition tempPos = readSampledPosition();

		x = tempPos.x;
		y = tempPos.y;
		xpx = tempPos.px;
		ypx = tempPos.py;
	}

	z1 = touchRead(TSC_MEASURE_Z1);
	z2 = touchRead(TSC_MEASURE_Z2);

	
	batt = touchRead(TSC_MEASURE_BATTERY);
	aux  = touchRead(TSC_MEASURE_AUX);

	// Read the time
	// rtcGetTime((uint8 *)ct);
	// BCDToInteger((uint8 *)&(ct[1]), 7);

	// Read the temperature
	temp = touchReadTemperature(&t1, &t2);

	// Update the IPC struct
	IPC->buttons		= but;
	IPC->touchX			= x;
	IPC->touchY			= y;
	IPC->touchXpx		= xpx;
	IPC->touchYpx		= ypx;
	IPC->touchZ1		= z1;
	IPC->touchZ2		= z2;
	IPC->battery		= batt;
	IPC->aux			= aux;

        /*
	for(i=0; i<sizeof(ct); i++) {
		IPC->time.curtime[i] = ct[i];
	}
        */

	IPC->temperature = temp;
	IPC->tdiode1 = t1;
	IPC->tdiode2 = t2;


	//sound code  :)
	TransferSound *snd = IPC->soundData;
	IPC->soundData = 0;

	if (0 != snd) {

		for (i=0; i<snd->count; i++) {
			s32 chan = getFreeSoundChannel();

			if (chan >= 0) {
				startSound(snd->data[i].rate, snd->data[i].data, snd->data[i].len, chan, snd->data[i].vol, snd->data[i].pan, snd->data[i].format);
			}
		}
	}

	Wifi_Update(); // update wireless in vblank

}

// callback to allow wifi library to notify arm9
void arm7_synctoarm9() { // send fifo message
   REG_IPC_FIFO_TX = 0x87654321;
}
// interrupt handler to allow incoming notifications from arm9
void arm7_fifo() { // check incoming fifo messages
   u32 msg = REG_IPC_FIFO_RX;

   if (msg == 0x87654321) {
     Wifi_Sync();
   } else if (msg == 0xDEADBEEF) {
     writePowerManagement(PM_CONTROL_REG, PM_POWER_DOWN);
   } else {
     NDSX_LedBlinkFifo(msg);
   }
}

//---------------------------------------------------------------------------------
int main(int argc, char ** argv) {
//---------------------------------------------------------------------------------
  REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR; // enable & prepare fifo asap
  // Reset the clock if needed
  rtcReset();

  //enable sound
  powerON(POWER_SOUND);
  SOUND_CR = SOUND_ENABLE | SOUND_VOL(0x7F);
  IPC->soundData = 0;

  irqInit();
  initClockIRQ();

  irqSet(IRQ_VBLANK, VblankHandler);
  irqSet(IRQ_WIFI, Wifi_Interrupt); // set up wifi interrupt

  irqEnable(IRQ_VBLANK | IRQ_WIFI);

  { // sync with arm9 and init wifi
    /* COMMENT OUT IN PRODUCTION 
    u32 fifo_temp;   

    while (1) { // wait for magic number
      while(REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY) 
        swiWaitForVBlank();

      fifo_temp=REG_IPC_FIFO_RX;

      if (fifo_temp==0x12345678) 
        break;
    }

    while (REG_IPC_FIFO_CR & IPC_FIFO_RECV_EMPTY) 
      swiWaitForVBlank();
    
    fifo_temp=REG_IPC_FIFO_RX; // give next value to wifi_init
    Wifi_Init(fifo_temp);
     COMMENT OUT IN PRODUCTION */

    irqSet(IRQ_FIFO_NOT_EMPTY,arm7_fifo); // set up fifo irq
    irqEnable(IRQ_FIFO_NOT_EMPTY);
    
    REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_RECV_IRQ;

    /* COMMENT OUT IN PRODUCTION 
    Wifi_SetSyncHandler(arm7_synctoarm9); // allow wifi lib to notify arm9
     COMMENT OUT IN PRODUCTION */
  } // arm7 wifi init complete

  // Keep the ARM7 out of main RAM
  while (1) 
    swiWaitForVBlank();
}


