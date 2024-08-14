import socket
import selectors
import sys
import types


HOST = "127.0.0.1"  # The server's hostname or IP address
PORT = 65432  # The port used by the server
sel = selectors.DefaultSelector()

Clients = [] 

class Client:
    def __init__(self,name,addr,sock):
        self.name = name
        self.addr = addr
        self.sock = sock
        
Clients.append(Client("echobot",None,None))

def accept_wrapper(sock):
    conn, addr = sock.accept()  # Should be ready to read
    print(f"Accepted connection from {addr}")
    conn.setblocking(False)
    data = types.SimpleNamespace(addr=addr, inb=b"", outb=b"")
    events = selectors.EVENT_READ | selectors.EVENT_WRITE
    sel.register(conn, events, data=data)
  

def service_connection(key, mask):
    sock = key.fileobj
    data = key.data
    if mask & selectors.EVENT_READ:
        recv_data = sock.recv(1)  
        if recv_data:
            data.outb += recv_data
            if recv_data.decode("utf-8")[-1] != "\n":
                while True:
                    recv_data = sock.recv(1)
                    if recv_data:
                        data.outb += recv_data
                    if recv_data.decode("utf-8")[-1] == "\n":
                        break
                

        else:
            print(f"Closing connection to {data.addr}")
            for user in Clients: 
                    if user.addr == data.addr:
                        Clients.remove(user)
           
            sel.unregister(sock)
            sock.close()
    if mask & selectors.EVENT_WRITE:
        if data.outb:
            
            SENT = False
            message_to_send = "" 
            message = (data.outb).decode("utf-8")
            if message[-1] == "\n":
               message = message[:-1]
         
            if message[:10] == "HELLO-FROM":
                name = message[11:]
                user = Client(name,data.addr,sock)
                if len(Clients) >= 64:
                    message_to_send = "BUSY\n"
                if len(name) == 0:
                    message_to_send = "BAD-RQST-BODY\n"
                for l in name:
                    if l == " " or l == "\n" or l == ",":
                        message_to_send = "BAD-RQST-BODY\n"
                if message_to_send == "":
                    for u in Clients:
                        if u.name == name:
                            message_to_send = "IN-USE\n"
                            break
                        
                    if message_to_send == "":
                        Clients.append(user)
                        message_to_send = "HELLO " + name + "\n"
                        

            elif message[:4] == "LIST":
                user_str = ""
                for c in Clients:
                    user_str += c.name + ","
                
                message_to_send = "LIST-OK " + user_str[:-1] +"\n"

            elif message[:4] == "SEND":
                i = 5
                for l in message[5:]: #seperate name from message
                    if l == " ":
                        break
                    i += 1
                target_name = message[5:i]
                msg = message[i+1:]
                SENDER = None
                if len(target_name) == 0:
                    message_to_send = "BAD-RQST-BODY\n"
                for l in target_name:
                    if l == "\n":
                        message_to_send = "BAD-RQST-BODY\n"

                if target_name == "echobot":
                    sent = sock.send(("SEND-OK\n").encode("utf-8")) 
                    data.outb = data.outb[sent:]
                    message_to_send = "DELIVERY echobot "+ msg + "\n"
                else: 
                    for user in Clients: 
                        if user.addr == data.addr:
                            SENDER = user
                    for TARGET in Clients:
                        if TARGET.name == target_name:
                            sent = sock.send(("SEND-OK\n").encode("utf-8")) 
                            data.outb = data.outb[sent:]

                            print("SENDING MESSAGE SOURCE: " + SENDER.name + " DEST: " + TARGET.name + " MSG: " + msg + "\n")
                            message_to_send = "DELIVERY " + SENDER.name + " " + msg + "\n"
                            data.outb = message_to_send.encode("utf-8")
                            
                            target_sock = TARGET.sock
                            sent = target_sock.send(data.outb)
                            data.outb = data.outb[sent:]
                            
                            SENT = True
                            break
                    if not SENT:
                        message_to_send = "BAD-DEST-USER\n"
            else:
                message_to_send = "BAD-RQST-HDR\n"


            if not SENT:
                
                data.outb = message_to_send.encode("utf-8")
                sent = sock.send(data.outb)  
                data.outb = data.outb[sent:]
            

lsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
lsock.bind((HOST, PORT))
lsock.listen()
print(f"Listening on {(HOST, PORT)}")
lsock.setblocking(False)
sel.register(lsock, selectors.EVENT_READ, data=None)

try:
    while True:
        events = sel.select(timeout=None)
        for key, mask in events:
            if key.data is None:
                accept_wrapper(key.fileobj)
            else:
                service_connection(key, mask)
except KeyboardInterrupt:
    print("Caught keyboard interrupt, exiting")
finally:
    sel.close()

            
             



