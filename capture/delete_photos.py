import os, sys
import subprocess

argslistfiles = ["gphoto2","--list-files"]

proc = subprocess.Popen(argslistfiles,stdout=subprocess.PIPE)
filelist = proc.stdout.read()
#print "LIST: " + filelist
dirloc = filelist.rfind("/store_00010001/DCIM/")
#print "DIR_LOC: " + str(dirloc)
storedirrm = filelist[dirloc:dirloc+29]
storedir = filelist[dirloc:dirloc+29] + "/"
print "STORE: " + storedir
argsdelete = ["gphoto2","--folder",storedir,"-d","1"]
argsdeletedir = ["gphoto2","--rmdir",storedirrm]
#argsdelete = ["gphoto2","--folder","/store_00010001/DCIM/101___01","-d","1"]
print argsdelete
    
while 1:
	proc = subprocess.Popen(argsdelete,stderr=subprocess.PIPE)
	wc = 0
	output = ''
	while proc.poll() == None:
		output = output + ' ' + proc.stderr.read()

	#output = proc.stderr.read()
	#print "RES: " + output
	#status = subprocess.call(argsdownload)
	#print status
	
	#fd = open("delete.txt")
	#lines =fd.readlines() 
	#fd.close()
	#print lines
	if output.find("Error") > 0:
		break

	print output


print "done"
proc = subprocess.call(argsdeletedir,stderr=subprocess.PIPE)
output = proc.stderr.read()
print output


