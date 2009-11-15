#include <nds.h>
#include <dswifi9.h>
#include <stdlib.h>

void wifi_timer(void) 
{
  Wifi_Timer(50);
}

void _arm9_synctoarm7() 
{
   REG_IPC_FIFO_TX = 0x87654321;
}

void arm9_fifo() 
{
  u32 value = REG_IPC_FIFO_RX;

  if (value == 0x87654321) {
    Wifi_Sync();
  }
}

void wifi_setup() 
{
  REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR; // enable & clear FIFO

  u32 Wifi_pass = Wifi_Init(WIFIINIT_OPTION_USELED);

  REG_IPC_FIFO_TX = 0x12345678;
  REG_IPC_FIFO_TX = Wifi_pass;

  *((volatile u16 *)0x0400010E) = 0; // disable timer3

  irqSet(IRQ_TIMER3, wifi_timer); // setup timer IRQ
  irqEnable(IRQ_TIMER3);

  irqSet(IRQ_FIFO_NOT_EMPTY, arm9_fifo); // setup fifo IRQ
  irqEnable(IRQ_FIFO_NOT_EMPTY);

  REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_RECV_IRQ; // enable FIFO IRQ

  Wifi_SetSyncHandler(_arm9_synctoarm7); // tell wifi lib to use our handler to notify arm7

  // set timer3
  *((volatile u16 *)0x0400010C) = -6553; // 6553.1 * 256 cycles = ~50ms;
  *((volatile u16 *)0x0400010E) = 0x00C2; // enable, irq, 1/256 clock

  while(Wifi_CheckInit() == 0) { // wait for arm7 to be initted successfully
    swiWaitForVBlank();  
  }
}

int wifi_connect() 
{
  Wifi_AutoConnect(); // request connect

  while(1) {
    int status = Wifi_AssocStatus(); // check status

    switch (status) {
      case ASSOCSTATUS_ASSOCIATED:
        return 0;

      case ASSOCSTATUS_CANNOTCONNECT:
        return -1;

      default:
        break;
    }
  }

  return -2;
}
