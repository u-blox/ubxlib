from collections import namedtuple
from base64 import b64decode
import binascii
import json
import sys
import requests
from Cryptodome.Cipher import AES


####################################################################################################
# vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv EDIT HERE vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv #
# API key and API secret as Base64 strings
API_KEY = ""
API_SECRET = ""
# Encrypted data returned by AT+USECE2EDATAENC as HEX string
ENC_DATA = ""
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ EDIT HERE ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ #
####################################################################################################


SERVER = "https://ssapi.services.u-blox.com"
DecParams = namedtuple('DecParams', 'Nonce, MACLength, CipherSuite, Key, ROTPublicUID')
CONTENT_TYPE = 'application/json'


def get_key_identity():
    if ENC_DATA[0] == '1':
        key_id = ENC_DATA[:32]
    elif ENC_DATA[0] == '2':
        key_id = ENC_DATA[:22]
    else:
        print("Encrypted data format unexpected!")
        sys.exit(1)
    print(f"Using key identity: {key_id}")
    return key_id


def authenticate():
    endpoint = "/v1/authenticate"
    payload = {
        "APIKey": API_KEY,
        "APISecret": API_SECRET,
    }
    headers = {
        'Content-Type': CONTENT_TYPE
    }
    print(f"Sending request to API {endpoint}")
    response = requests.request("POST", SERVER+endpoint, headers=headers, data=json.dumps(payload))

    if response.ok:
        try:
            jresp = response.json()
            print("The request succeeded!")
            return (jresp['AuthToken'], jresp['RefreshToken'])
        except ValueError:
            print(f"Error parsing the response: {response.text.encode('utf8')}")
    else:
        print("The request failed!")
        print(response.text.encode('utf8'))


def get_e2e_decrypt_params(auth_token):
    endpoint = "/v1/e2e/uplink/protectionparameters/get"
    payload = {
        "EncryptedHeader": get_key_identity()
    }
    headers = {
        'Authorization': auth_token,
        'Content-Type': CONTENT_TYPE
    }
    print(f"Sending request to API {endpoint}")
    response = requests.request("POST", SERVER+endpoint, headers=headers, data=json.dumps(payload))

    if response.ok:
        try:
            jresp = response.json()
            print("The request succeeded!")
            print(jresp)
            return DecParams(**jresp)
        except ValueError:
            print(f"Error parsing the response: {response.text.encode('utf8')}")
    else:
        print("The request failed!")
        print(response.text.encode('utf8'))


def aesccm_dec(dec_params):
    # AES-128-CCM
    # decryption key and nonce are given by the API response as base64 strings
    # they need to be converted to bytes (binary data)
    key = b64decode(dec_params.Key)
    nonce = b64decode(dec_params.Nonce)
    # ciphertext and MAC tag are extracted from the encrypted data as hex strings
    # get key identity and MAC tag lengths
    mac_len = dec_params.MACLength // 8             # in bytes
    key_id_len = 16 if ENC_DATA[0] == '1' else 11     # in bytes
    # they need to be converted to bytes (binary data)
    ciphertext = binascii.unhexlify(ENC_DATA[key_id_len*2:-mac_len*2])
    tag = binascii.unhexlify(ENC_DATA[-mac_len*2:])

    try:
        cipher = AES.new(key, AES.MODE_CCM, nonce=nonce)
        plaintext = cipher.decrypt_and_verify(ciphertext, tag)
        print("The message was: " + plaintext.decode('utf-8'))
    except (ValueError, KeyError) as ex:
        print("Incorrect decryption")
        print(ex)


if __name__ == '__main__':
    auth_token, refresh_token = authenticate()
    if auth_token is None:
        print("Can't authenticate to security server!")
        sys.exit(0)

    dec_params = get_e2e_decrypt_params(auth_token)
    if dec_params is None:
        print("Can't get decryption parameters!")
        sys.exit(0)

    if dec_params.CipherSuite == "AES_128_CCM":
        print("V1: using AES-128-CCM decryption")
        aesccm_dec(dec_params)
    else:
        print("V2 not supported")
        sys.exit(0)
