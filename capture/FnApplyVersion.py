#!/usr/bin/env fnpython
import os,sys

fd = open("Source/CS6/Effect/Kronos/KronosVersion.h")
lines = fd.readlines()
fd.close()

fd = open("Source/CS6/Effect/Kronos/KronosVersion.h", "w")
for i in lines:
  if i.startswith("#define GIT_VERSION"):
    #Hack - previous versions have a lower build number from before getting this onto jenkins.
    num = int(os.environ.get("BUILD_NUMBER", "0"))
    num += 5
    fd.write("#define GIT_VERSION " + str(num) + "\n")
  else:
    fd.write(i)
fd.close()
