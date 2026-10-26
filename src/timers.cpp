#include "timers.h"
#include "storage.h"
#include <Arduino.h>

#include "app_state.h"
#include "config.h"

namespace
{
  volatile uint8_t halfSecondTicks = 0;
}

void configureLogTimer()
{
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC5);
  TC_Configure(TC1, 2, TC_CMR_TCCLKS_TIMER_CLOCK5 | TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC);
  TC_SetRC(TC1, 2, TIMER_RC);
  TC1->TC_CHANNEL[2].TC_IER = TC_IER_CPCS;
  TC1->TC_CHANNEL[2].TC_IDR = ~TC_IER_CPCS;
  NVIC_ClearPendingIRQ(TC5_IRQn);
  NVIC_EnableIRQ(TC5_IRQn);
  TC_Start(TC1, 2);
}

void TC5_Handler()
{
  TC_GetStatus(TC1, 2);
  // Keep the interrupt short: the main loop performs the filesystem write.
  halfSecondTicks++;
  if (halfSecondTicks >= TICKS_PER_LOG)
  {
    halfSecondTicks = 0;
    logRequested = true;
  }
}

void initializeLedTimer()
{
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC6);
  TC_Configure(TC2, 0, TC_CMR_TCCLKS_TIMER_CLOCK4 | TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC);
  TC_SetRC(TC2, 0, 32813);
  TC2->TC_CHANNEL[0].TC_IDR = 0xFFFFFFFF;
  TC2->TC_CHANNEL[0].TC_IER = TC_IER_CPCS;
  NVIC_EnableIRQ(TC6_IRQn);
  TC_Start(TC2, 0);
}

void TC6_Handler()
{
  TC_GetStatus(TC2, 0);
  if (sdi12Active)
  {
    sdi12BlinkCount++;
    if (sdi12BlinkCount >= 2)
    {
      sdi12BlinkCount = 0;
      sdi12State = !sdi12State;
      digitalWrite(SDI_12_STATUS_LED_PIN, sdi12State ? HIGH : LOW);
    }
  }
  else
  {
    sdi12BlinkCount = 0;
    sdi12State = false;
    digitalWrite(SDI_12_STATUS_LED_PIN, LOW);
  }
}

void InitialiseHardwareTimerForSensorSampling()
{
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(static_cast<uint32_t>(TC4_IRQn));
  TC_Configure(TC1, 1, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK4);
  uint32_t rc = VARIANT_MCK / 128 / 20;
  TC_SetRC(TC1, 1, rc);
  TC1->TC_CHANNEL[1].TC_IER = TC_IER_CPCS;
  TC1->TC_CHANNEL[1].TC_IDR = ~TC_IER_CPCS;
  NVIC_EnableIRQ(TC4_IRQn);
  TC_Start(TC1, 1);
}

void TC4_Handler()
{
  TC_GetStatus(TC1, 1);
  // Sampling work runs in the main loop so I2C calls never happen in an ISR.
  isTimerForSensorSamplingTriggered = true;
}
