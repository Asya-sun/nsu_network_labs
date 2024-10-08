import socket
import threading
from pathlib import Path
import os
import string
import time
import math

#Variable for holding information about connections
connections = []
BYTES_TO_RECEIVE = 1460
path = Path("uploads")


mutex = threading.Lock()
#Client class, new instance created for each connected client
class Client(threading.Thread):
    def __init__(self, socket, address):
        threading.Thread.__init__(self)
        self.socket = socket
        self.address = address
        self.packets_number = 0
        self.sending_times = []
        self.bytes_number = 0
        self.total_bytes_number = 0
    
    def get_transfer_rate(self):
        if len(self.sending_times) <= 1:
            return 0
        delta_time = self.sending_times[-1] - self.sending_times[0]
        return math.ceil(self.bytes_number /delta_time * 100) / 100
    
    def get_client_address(self):
        return self.address
    
    def run(self):
        while True:
            try:
                data = self.socket.recv(BYTES_TO_RECEIVE)
                self.packets_number +=1
                self.sending_times.append(time.time())
                self.bytes_number += len(data)

            except:
                print("Client " + str(self.address) + " has disconnected")
                success = 0
                self.socket.sendto(success.to_bytes(), self.address)
                mutex.acquire()
                connections.remove(self)
                mutex.release()
                break

            if self.packets_number == 1:
                #in first packet name of file + its size must be sent splitted by " " 
                meta_inf = data.decode("utf-8").split(" ")
                new_path = os.path.join(path, meta_inf[0])
                print("new_path " + new_path)
                # check if file already exists and delete it if yes
                if (os.path.isfile(new_path)):
                    print("removing existing file " + new_path)
                    os.remove(new_path)
                file = open(new_path, "w+b")
                print("file " + new_path + " was opened")
                self.total_bytes_number = int(meta_inf[1])
                self.bytes_number = 0
                print("total_bytes_number " + str(self.total_bytes_number) + "bytes_number" + str(self.bytes_number))
            elif self.bytes_number == self.total_bytes_number:
                file.write(data)
                print("Client successfully sent a file and left the chanel " + self.address[0])
                success = 1
                self.socket.sendto(success.to_bytes(1, byteorder='big'), self.address)
                self.socket.close()
                file.close()
                mutex.acquire()
                connections.remove(self)
                mutex.release()
                break
            else:
                file.write(data)
                
                

#Wait for new connections
def new_connection(socket):
    while True:
        sock, address = socket.accept()
        connections.append(Client(sock, address))
        connections[-1].start()
        print("New connection at ID " + str(address)) #little doubtful


def display_transfer_rate():
    if (len(connections) == 0):
        displayTimer = threading.Timer(1, display_transfer_rate)
        displayTimer.start()
        return
    print("Transfer rate of clients' connections")
    mutex.acquire()
    for connection in connections:
        print(str(connection.get_client_address()) + "      " + str(connection.get_transfer_rate()))
    mutex.release()
    displayTimer = threading.Timer(1, display_transfer_rate)
    displayTimer.start()


transfer_rate_displayer = threading.Timer(1, display_transfer_rate)


def main():
    #Get host and port
    host = input("Host: ")
    port = int(input("Port: "))
    # host = 'localhost'
    # port = 12345
    print(host + " is listening")

    #Create new server socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    sock.listen(5)

    #Create new thread to wait for connections
    new_connections_thread = threading.Thread(target = new_connection, args = (sock,))
    new_connections_thread.start()


    host = socket.gethostname()

    transfer_rate_displayer.start()

    if not os.path.exists(path):
        os.mkdir(path)    
main()