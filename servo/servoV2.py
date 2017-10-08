import socket
import sys
from Adafruit_PWM_Servo_Driver import PWM
import time

HOST = ''   # Symbolic name meaning the local host
PORT = 24169    # Arbitrary non-privileged port
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print 'Socket created'
try:
    s.bind((HOST, PORT))
except socket.error , msg:
    print 'Bind failed. Error code: ' + str(msg[0]) + 'Error message: ' + msg[1]
    sys.exit()
print 'Socket bind complete'
s.listen(1)
print 'Socket now listening'

ang_pan = 30.0
ang_tilt = 20.0
m =  5.65
c = 170
tiltLock = 44 #50
tiltOpen = 90
panLock = 50 #60
panOpen = 0

pwm = PWM(0x40, debug=True)
pwm.setPWMFreq(70)

CtiltLock = m*tiltLock + c
CtiltOpen = m*tiltOpen + c
CpanLock = m*panLock + c
CpanOpen = m*panOpen + c

while 1:
    conn, addr = s.accept()
    print 'Connected with ' + addr[0] + ':' + str(addr[1])
    data = conn.recv(1024)
    angle_array = data.split(',');
    reply = 'Received... ' + data
    if(data == 'quit'):
        conn.send(data)
        conn.close()
        s.close()
        break
    
    if angle_array[0] == 'S':
        angle_pan = int(angle_array[1])
        angle_tilt = int(angle_array[2])
        
        if angle_pan >= 0 and angle_pan <= 100:
            #ang_pan = angle_pan + 40
            ang_pan = angle_pan
        
        if angle_tilt >= 0 and angle_tilt <= 100:
            ang_tilt = angle_tilt
            #ang_tilt = angle_tilt
    
    Ctilt = int(round(ang_tilt*m + c))
    Cpan = int(round(ang_pan*m + c))
    pwm.setPWM(0, 0, int(CtiltOpen))
    pwm.setPWM(2, 0, int(CpanOpen))

    time.sleep(1)
    #pwm.setPWM(0, 0, int(Ctilt))
    pwm.setPWM(1, 0, int(Ctilt))
    time.sleep(1)
    #pwm.setPWM(2, 0, int(Cpan))
    pwm.setPWM(3, 0, int(Cpan))
    time.sleep(3)
    pwm.setPWM(0, 0, int(CtiltLock))    
    pwm.setPWM(2, 0, int(CpanLock))
    time.sleep(5)
    pwm.setPWM(1, 0, 0)
    pwm.setPWM(3, 0, 0)
    pwm.setPWM(2, 0, 0)
    pwm.setPWM(0, 0, 0)

    print('pan: ' + str(Cpan) + ' tilt: ' + str(Ctilt)) 
    reply = str(ang_pan) + ',' + str(ang_tilt)
    if not data: break
    conn.send(reply)
    
    
conn.close()
s.close()
