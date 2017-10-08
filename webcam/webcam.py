import socket
import sys
import time
import os
import subprocess

from time import gmtime, strftime
from time import gmtime, strftime


print 'Running SouthWest camera'

argsproc = ["vidcap"]   
run_count = 1
server_ip = '192.168.1.10'
server_port = 17213
videodir = '/home/gary/testvid/'
#videodir = 'd:\\Panda\\p\\'
videodir0 = videodir + 'video0/'

#videodir = 'd:\\Panda\\p\\'

def send_response(host,port,msg) :    
    print host + ":"+ str(port)
    
    sc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sc.settimeout(2)
     
    # connect to remote host
    try :
        sc.connect((host, port))
    except :
        print 'Unable to connect'
        return
     
    print 'Connected to remote host. Start sending messages'
    print "message: " + msg
    sc.send(msg)
    #data = sc.recv(4096)
    #print data
    sc.close()
 

    
while(1) :
    if os.path.isdir(videodir) :
        print 'Video path exist!'
    else :
        print 'Video path not active!'
        time.sleep(1)
        #continue
    
    
    #msg = 'WCAM#0'
    #send_response(server_ip,server_port,msg)
    print 'webcam process start ' + str(run_count) + ' at ' + strftime("%Y-%m-%d %H:%M:%S", gmtime())
    status = subprocess.call(argsproc)
    run_count = run_count + 1
    time.sleep(3)
