import time
import os, sys
import subprocess
#os.system(sys.executable)

argsshoot = ["ptpcam","--chdk=""lua shoot()"""]
#argspwd = ["pwd"]
argsdownload = ["gphoto2","--folder","/store_00010001/DCIM/100___01/","-p","1"]
argsdelete = ["gphoto2","--folder","/store_00010001/DCIM/100___01/","-d","1"]
print argsdownload
status = subprocess.call(argsshoot)
print status
time.sleep(5)

#proc = subprocess.Popen(argspwd,stdout=subprocess.PIPE)
#outputpwd = proc.stdout.read()
outputpwd = os.getcwd()
picdir = outputpwd + '/pics/'
os.chdir(picdir)
#subprocess.call(["cd",outputpwd+"/pic/"])
proc = subprocess.Popen(argsdownload,stderr=subprocess.PIPE)
output = proc.stderr.read()
#proc = subprocess.Popen(argspwd,stdout=subprocess.PIPE)
#outputpwd = proc.stdout.read()
print "current dir: " + outputpwd
print "picdir: " + picdir
print "C: " + output
proc = subprocess.Popen(argsdelete,stderr=subprocess.PIPE)
outputdel = proc.stderr.read()
print "DEL: " + outputdel


