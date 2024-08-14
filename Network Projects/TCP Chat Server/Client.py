
import socket
import threading


sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

HOST =  "127.0.0.1" 
PORT = 65432

host_port = ("127.0.0.1", 65432)
sock.connect(host_port)

def send_bytes(message):
    encoded = message.encode("utf-8")
    bytes_len = len(message)
    num_bytes_to_send = bytes_len
    while num_bytes_to_send > 0:
        num_bytes_to_send -= sock.send(encoded[bytes_len-num_bytes_to_send:])


def recieve_bytes():
    data = sock.recv(4096)
    messages = []
    if not data:
        print("Socket is closed.")
    decoded = data.decode("utf-8")
    if decoded[-1] != "\n":
        while decoded[-1] != "\n":
            data += sock.recv(4096)
            decoded = data.decode("utf-8")
    for i in range(decoded.count("\n")):
        index = decoded.find("\n")
        messages.append(decoded[:index+1])
        decoded = decoded[index+1:]
    return messages

def message_for_u():
    i = recieve_bytes()
    for incoming in i:
        if incoming[:8] == "DELIVERY":
            print(incoming[9:])
        elif incoming[:7] == "LIST-OK":
            users = incoming[:-1] + incoming[-1][:-1]
            user_list = users[8:].split(",")
            print("All users online: ")
            for i in user_list:
                if i != user_list[-1]:
                    print(i + ", ", end="")
                else:
                    print(i)
        elif incoming == "SEND-OK\n":
            print("successfully sent")
        elif incoming == "BAD-DEST-USER\n":
            print("user not online")
        elif incoming == "BUSY\n":
            print("server is busy")
        elif incoming == "BAD-RQST-HDR\n":
            print("error in header")
        elif incoming == "BAD-RQST-BODY\n":
            print("error in body")
    message_for_u()   
        
     
  


while (True):
    sock.close
    print("Enter your name: ")
    name = input()
    send_bytes("HELLO-FROM " + name + "\n")  
    message = recieve_bytes()
    if message[0] =="IN-USE\n":
        print("Username is already taken!\n")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(host_port)
    elif message[0] == "BAD-RQST-BODY\n":
        print("error in name")
    elif message[0] == "BUSY\n":
        print("Maximum number of users are logged on!\nPlease try again later")
        exit()
    else:
        break

print("youre connected!!")

print("type '!who' to view all currently logged-in users\n")
print("type '!quit' to quit\n")
print("send messages to other users by typing @username message\n")


def interface():
    t1 = threading.Thread(daemon = True,target= message_for_u)
    t1.start()
    command = input()

    if command == "!who":
        send_bytes("LIST\n")
    elif command == "!quit":
        print("Bye!")
        return command
    elif command[0] == "@":
        send_bytes("SEND " + command[1:] + "\n") 
    else:
        print("invalid command")
    return command

def system():
    command = ""
    while command != "!quit":
        command = interface()

 

system()


