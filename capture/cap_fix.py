import socket
import sys
import time
import os
import subprocess

def capture_image(pic_dir) :
    argsshoot = ["ptpcam","--chdk=""lua shoot()"""]
    argslistfiles = ["gphoto2","--list-files"]   
    #argsdownload = ["gphoto2","--folder","/store_00010001/DCIM/100___01/","-p","1"]
    #argsdelete = ["gphoto2","--folder","/store_00010001/DCIM/100___01/","-d","1"]
    #print argsdownload
    status = subprocess.call(argsshoot)
    print status
    time.sleep(5)
    proc = subprocess.Popen(argslistfiles,stdout=subprocess.PIPE)
    filelist = proc.stdout.read()
    #print "LIST: " + filelist
    dirloc = filelist.find("/store_00010001/DCIM/")
    #print "DIR_LOC: " + str(dirloc)
    storedir = filelist[dirloc:dirloc+29] + "/"
    print "STORE: " + storedir
    argsdownload = ["gphoto2","--folder",storedir,"-p","1"]
    argsdelete = ["gphoto2","--folder",storedir,"-d","1"]
    print argsdownload
    #return filelist
    outputpwd = os.getcwd()
    picdir = outputpwd + pic_dir
    if os.path.isdir(picdir) :
        os.chdir(picdir)
    else:
        return 'FILE'
       
    
    proc = subprocess.Popen(argsdownload,stderr=subprocess.PIPE)
    output = proc.stderr.read()   
    print "current dir: " + outputpwd
    print "picdir: " + picdir
    print "C: " + output
    download_parts = output.split(' ')
    file_name = 'FILE'
    if download_parts[0] == 'Downloading' :
        file_name = download_parts[1]
        file_name = file_name[1:-1]
        
    print "filename: " + file_name
    proc = subprocess.Popen(argsdelete,stderr=subprocess.PIPE)
    outputdel = proc.stderr.read()
    os.chdir(outputpwd)
    print "DEL: " + outputdel
    return file_name

    
     
    
pic_dir = '/pics/'
filename = capture_image(pic_dir)
    
