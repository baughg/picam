# telnet program example
import socket, select, string, sys
 
def prompt() :
    sys.stdout.write('<You> ')
    sys.stdout.flush()
 
def send_response(host,port,msg) :
    #host = '192.168.1.63'
    #port = 24069
     
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(2)
     
    # connect to remote host
    try :
        s.connect((host, port))
    except :
        print 'Unable to connect'
        sys.exit()
     
    print 'Connected to remote host. Start sending messages'
    
    s.send(msg)
    data = s.recv(4096)
    print data
    s.close()
#main function
if __name__ == "__main__":
     
    if(len(sys.argv) < 3) :
        print 'Usage : python telnet.py hostname port'
        sys.exit()
     
    host = sys.argv[1]
    port = int(sys.argv[2])
     
    #prompt()
     
    while 1:
        msg = "Hello World"
        send_response(host,port,msg)
        #s.send(msg)
        #data = s.recv(4096)
        #print data
        #s.close()
        break
        