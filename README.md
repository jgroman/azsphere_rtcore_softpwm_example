# Generating Software PWM using Azure Sphere Real-Time Core Application

PWM (or Pulse Width Modulation) is basically a clock signal with a programmable duty cycle. PWM generator could then be decribed by using two values: clock frequency (or period) and current duty cycle percentage. This implementation uses real-time M4 core GPT timers for generating clock signal where timer duration is set depending on duty cycle value. GPT timer after running out generates interrupt and the interrupt handling routine flips GPIO state. 

Since the original GPT timer setup routine supports only 1 kHz timer clock, which is probably too slow for PWM generating purposes, I have added support for GPT timer clock at maximum supported 32 kHz. See [mt3620-timer-user.c](https://github.com/jgroman/azsphere_rtcore_softpwm_example/blob/master/rtcore_softpwm_example/mt3620-timer-user.c) with added basic routines for previously unsupported GPT2 timer. GPT2 is a free-running timer and cannot be used for interrupts, but it can be very useful for time keeping as an Arduino's *millis()* and *delay()* function replacement. With this setup we can reach maximum software PWM frequencies around 5 kHz.

The repository contains complete real-time core application which demonstrates PWM function by dimming RGB LED after repeated button1 presses. 
