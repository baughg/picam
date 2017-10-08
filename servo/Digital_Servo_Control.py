#!/usr/bin/python
import sys
from Adafruit_PWM_Servo_Driver import PWM
import time

# ===========================================================================
# Example Code
# ===========================================================================

# Initialise the PWM device using the default address
# bmp = PWM(0x40, debug=True)
pwm = PWM(0x40, debug=True)

servoMin = 150  # Min pulse length out of 4096
servoMax = 600  # Max pulse length out of 4096
servoSet = 333
def setServoPulse(channel, pulse):
  pulseLength = 1000000                   # 1,000,000 us per second
  pulseLength /= 60                       # 60 Hz
  print "%d us per period" % pulseLength
  pulseLength /= 4096                     # 12 bits of resolution
  print "%d us per bit" % pulseLength
  pulse *= 1000
  pulse /= pulseLength
  pwm.setPWM(channel, 0, pulse)

pwm.setPWMFreq(70)                        # Set frequency to 60 Hz
angle = 90.0
anglepan = 90.0
m = 3.0

Ctilt = round(angle*m + 60)
Cpan = round(anglepan*m + 60)
#Cc = round(-(angle)*m + 600)
print("angles " + str(angle) + "' [T: " + str(Ctilt) + ", P: " + str(Cpan) + "]")
pwm.setPWM(0, 0, int(Ctilt))
pwm.setPWM(1, 0, int(Cpan))
pwm.setPWM(2, 0, int(Cpan))
