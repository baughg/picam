import socket
import sys
import time
import os
import subprocess

prevstoredir = ''

def set_zoom(zoom_level) :
    zoom_str = "--chdk=lua set_zoom(" + str(zoom_level) + ")"
    print 'Zoom command: ' + zoom_str
    argszoom = ["ptpcam",zoom_str]
    argsshoot = ["ptpcam","--chdk=""lua set_zoom(30)"""]
    print "this: " + argsshoot[1]
    status = subprocess.call(argszoom)
    time.sleep(2)		


def capture_image(pic_dir) :
    global prevstoredir
    argsshoot = ["ptpcam","--chdk=""lua shoot()"""]
    argslistfiles = ["gphoto2","--list-files"]   
    #argsdownload = ["gphoto2","--folder","/store_00010001/DCIM/100___01/","-p","1"]
    #argsdelete = ["gphoto2","--folder","/store_00010001/DCIM/100___01/","-d","1"]
    #print argsdownload
    status = subprocess.call(argsshoot)
    print status
    time.sleep(4.5)
    proc = subprocess.Popen(argslistfiles,stdout=subprocess.PIPE)
    wc = 0
    filelist = ''
    while proc.poll() == None:
	filelist = filelist + ' ' + proc.stdout.read()
	wc = wc + 1
    
    #filelist = proc.stdout.read()
    #print "LIST: " + filelist
    dirloc = filelist.rfind("/store_00010001/DCIM/")
    jpgfile = filelist.rfind(".JPG")
    attemptcount = 0
    print 'LISTING: ' + filelist

    while(jpgfile == -1) :
        proc = subprocess.Popen(argslistfiles,stdout=subprocess.PIPE)
	wc = 0
	filelist = ''
    	while proc.poll() == None:
		filelist = filelist + ' ' + proc.stdout.read()
		wc = wc + 1
    
        #filelist = proc.stdout.read()
        dirloc = filelist.rfind("/store_00010001/DCIM/")
        jpgfile = filelist.rfind(".JPG")
        if(jpgfile != -1) :
	    break
	time.sleep(3)
        attemptcount = attemptcount + 1
        print 'wait: ' + str(attemptcount)
        if(attemptcount >= 10) :
	    return "FILE_EMPTY"
        
    #print "DIR_LOC: " + str(dirloc)
    storedirrm = filelist[dirloc:dirloc+29]
    storedir = filelist[dirloc:dirloc+29] + "/"
    print "STORE: " + storedir
    argsdownload = ["gphoto2","--folder",storedir,"-p","1"]
    argsdelete = ["gphoto2","--folder",storedir,"-d","1"]
    argsdeletedir = ["gphoto2","--rmdir",prevstoredir]
    
    print argsdownload
    #return filelist
    outputpwd = os.getcwd()
    picdir = outputpwd + pic_dir
    if os.path.isdir(picdir) :
        os.chdir(picdir)
    else:
        print "Path error: " + picdir
        os.chdir(outputpwd)
        umountpath = ["umount","-a"]
        mountpath = ["mount","-a"]

        proc = subprocess.call(argsdelete,stderr=subprocess.PIPE)
        status = subprocess.call(umountpath)
        time.sleep(3)
        status = subprocess.call(mountpath)	
        return 'PATH'
       
    
    proc = subprocess.call(argsdownload,stdout=subprocess.PIPE)
    output = proc.stdout.read()   
    print "current dir: " + outputpwd
    print "picdir: " + picdir
    print "C: " + output
    download_parts = output.split(' ')
    file_name = 'FILE'
    if download_parts[0] == 'Saving' :
        file_name = download_parts[3]
        file_name = file_name[0:-1]
        
    print "filename: " + file_name
    proc = subprocess.Popen(argsdelete,stderr=subprocess.PIPE)
    wc = 0
    outputdel = ''
    while proc.poll() == None:
	wc = wc + 1
    	outputdel = outputdel + ' ' + proc.stderr.read()
    
    #outputdel = proc.stderr.read()
    os.chdir(outputpwd)
    print "DEL: " + outputdel
    if(prevstoredir != storedirrm and len(prevstoredir) > 0) :
	print 'removing dir: ' + prevstoredir
    	proc = subprocess.Popen(argsdeletedir,stderr=subprocess.PIPE)
    	wc = 0
	outputdel = ''
    	while proc.poll() == None:
		wc = wc + 1
    		outputdel = outputdel + ' ' + proc.stderr.read()
    
	#outputdel = proc.stderr.read()
	print outputdel

    prevstoredir = storedirrm
    return file_name

def send_response(host,port,msg) :
    #host = '192.168.1.63'
    #port = 24069
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
    sc.shutdown(socket.SHUT_RDWR)
    sc.close()
    
HOST = ''   # Symbolic name meaning the local host
PORT = 24070    # Arbitrary non-privileged port
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

pan_angle =30
tilt_angle = 20
zoom_level = 0

argsmode = ["ptpcam","--chdk=""mode 1"""]
status = subprocess.call(argsmode)
time.sleep(5)
argsfocus = ["ptpcam","--chdk=""lua set_focus(1000)"""]
argsfocusman = ["ptpcam","--chdk=""lua set_aflock(1)"""]
argsfocusman2 = ["ptpcam","--chdk=""lua set_mf(1)"""]

status = subprocess.call(argsfocusman2)
time.sleep(2)
status = subprocess.call(argsfocusman)
time.sleep(2)
status = subprocess.call(argsfocus)

print 'Camera ready!'

while 1:
    conn, addr = s.accept()
    print 'Connected with ' + addr[0] + ':' + str(addr[1])
    data = conn.recv(1024)
    
    if(data == 'quit'):
        conn.send(data)
        conn.close()
        s.close()
        break
        
    
    command_array = data.split('#')
    pic_rel_dir = command_array[0]
    servo_cmd = command_array[1]
    servo_cmd_next = command_array[2]
    reference_str = command_array[3]
    
    servo_param = servo_cmd.split(',')
    pan_ang = int(servo_param[1])
    tilt_ang = int(servo_param[2])
    zoom_lvl = int(servo_param[3])

    if((pan_ang != pan_angle) or (tilt_ang != tilt_angle)) :
        pan_angle = pan_ang
        tilt_angle = tilt_ang
        msg = 'S,' + str(pan_angle) + ',' + str(tilt_angle)
        #send_response('127.0.0.1',24169,msg)
        print 'moving camera to - pan: ' + str(pan_angle) + ', tilt: ' + str(tilt_angle)

    
    if(zoom_lvl != zoom_level) :
       zoom_level = zoom_lvl
       set_zoom(zoom_level)

    servo_param_next = servo_cmd_next.split(',')
    pan_ang_next = int(servo_param_next[1])
    tilt_ang_next = int(servo_param_next[2])
    
    print "pic dir rel: " + pic_rel_dir
    print "angles: p -> " + str(pan_ang) + " t -> " + str(tilt_ang)
    print "angles next: p -> " + str(pan_ang_next) + " t -> " + str(tilt_ang_next)
    reply = 'Received... ' + data
    
    
    print reply
    
    
    #if not data: break
    conn.send(reply)
    
    pic_dir = '/pics/' + pic_rel_dir
    filename = capture_image(pic_dir)
    msg = filename + ",""" + reference_str + ""
    send_response(addr[0],17213,msg)
    
conn.close()
s.close()
