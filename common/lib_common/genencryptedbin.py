#! /usr/bin/python3

"""
This script encrypts a binary. 

"""

import sys
import os
from cryptography.hazmat.primitives.ciphers.aead import AESCCM
import secrets

""" Randomly generated key need to be stored somewhere safe """
key = bytes([0xeb, 0x09, 0x70, 0xb1, 0xa7, 0x7d, 0x1b, 0x4b, 0xf7, 0xfb, 0x46, 0x1f, 0xc6, 0xaa, 0x0c, 0x92])

""" Nonce shall be generated each time. A key/nonce pair shall never be used more than once. """
""" The nonce length must match the length used by the decryptor """
nonce = secrets.token_bytes(13)

""" The tag size must match the size used by the decryptor """
tag_size = 16

def main():

  if len(sys.argv) <= 4:
    print("Need path to binary file and library name.")
    print("Exiting...")
    sys.exit()

  filepath_aad = sys.argv[1] # authenticated associated data
  filepath = sys.argv[2]
  use_encryption = sys.argv[3]
  libname = sys.argv[4]

  if not os.path.isfile(filepath):
    print("File {} does not exist. Exiting...".format(filepath))
    sys.exit()

  if not os.path.isfile(filepath_aad):
    print("File {} does not exist. Exiting...".format(filepath_aad))
    sys.exit()

  with open(filepath, "rb") as f:
    data = f.read()

  with open(filepath_aad, "rb") as f:
    data_aad = f.read()

  if use_encryption == "1":
    aesccm = AESCCM(key,tag_size)
    # Put the nonce at the start of the file.
    data_enc = nonce + aesccm.encrypt(nonce,data,data_aad)
    with open(libname,"wb") as f:
       f.write(data_enc)  
  else:
    with open(libname,"wb") as f:
       f.write(data)  
    
if __name__ == '__main__':
   main()
