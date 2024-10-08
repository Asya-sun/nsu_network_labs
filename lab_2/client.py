import sys
import socket
import threading
import os
from time import sleep
import math

BYTES_TO_RECEIVE = 1460

def receive(socket):
    while True:
        # maybe try except isn't needed here
        try:
            data = socket.recv(BYTES_TO_RECEIVE)
            print(str(data.decode("utf-8")))
            if data[0] == 1:
                print("receiving ended successfully")
                sys.exit(0)
            else:
                print("receiving went bad")
                sys.exit(0)
        except:
            print("You have been disconnected from the server")
            signal = False
            break


def main():
    #Get host and port
    host = input("Host: ")
    port = int(input("Port: "))
    # host = 'localhost'
    # port = 12345
    addr = (host,port)
    file_name = input("enter your file name: ")

    #Attempt connection to server
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.connect(addr)
    except:
        print("Could not make a connection to the server")
        input("Press enter to quit")
        sys.exit(0)

    #Create new thread to wait for data
    receiveThread = threading.Thread(target = receive, args = (sock, ))
    receiveThread.start()


    file = open(file_name, "rb")
    file_size = os.path.getsize(file_name)

    first_send_string = file_name + " " + str(file_size)
    print("first packet: " + first_send_string)
    sock.sendall(str.encode(first_send_string))
    sleep(0.1)

    for i in range(0, math.ceil(file_size/BYTES_TO_RECEIVE)):
        data = file.read(BYTES_TO_RECEIVE)
        sock.sendall(data)
        sleep(0.1)
    print("the file was sent")

main()