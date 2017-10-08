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
m = 3.0
outval=servoSet 

dir = -1 
while (True):
  # Change speed of continuous servo on channel O
  if(dir == -1):
   angle_target = angle-5
   inc = -1
  
  if(dir == 1):
   angle_target = angle+5
   inc = 1

  if(angle_target < 5 and dir == -1):
    angle_target = 5
    inc = 1
    dir = 1

  if(angle_target > 175 and dir == 1):
    angle_target = 175
    inc = -1
    dir = -1

  while (dir==1 or (angle > angle_target)) and (dir == -1 or (angle < angle_target)):
    C = round(angle*m + 60)
    Cc = round(-(angle)*m + 600)
    print("angles " + str(angle) + "' [" + str(C) + "," + str(Cc) + "]")
    pwm.setPWM(0, 0, int(C))
    pwm.setPWM(1, 0, int(Cc))
    pwm.setPWM(2, 0, int(Cc))
    angle = angle+inc
  #time.sleep(1)
  #pwm.setPWM(0, 0, servoMax)
  #time.sleep(1)
  input = sys.stdin.readline()
  #print("angles " + str(angle) + "' [" + str(C) + "," + str(Cc) + "]")
  #angle = angle - 5
  #if(angle < 5.0):
    #angle = 175

