#!/usr/bin/env python3
"""UDP echo/reply script — reliable replacement for nc -u -l"""
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', 2000))
print('[listening on UDP 0.0.0.0:2000]')

data, addr = s.recvfrom(1024)
print(f'[recv from {addr[0]}:{addr[1]}] {data.decode()}')

reply = input('reply> ')
s.sendto(reply.encode(), addr)
print(f'[sent {len(reply)} bytes to {addr[0]}:{addr[1]}]')
s.close()
