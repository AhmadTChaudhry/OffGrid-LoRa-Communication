#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <Arduino.h>

static const char* ENCRYPTION_KEY = "SecureLoraComms1"; 

// XOR STRING ENCRYPTION
String encryptMessage(const String& plaintext) {
    if (plaintext.length() == 0) {
        return "";
    }

    String encrypted = "";
    int keyLength = strlen(ENCRYPTION_KEY);

    for (int i = 0; i < plaintext.length(); i++) {
        char plainChar = plaintext.charAt(i);
        char keyChar = ENCRYPTION_KEY[i % keyLength];
        char encryptedChar = plainChar ^ keyChar;
        
        // Convert encrypted char to HEX string
        if ((byte)encryptedChar < 16) {
            encrypted += "0";
        }
        encrypted += String((byte)encryptedChar, HEX);
    }
    
    return encrypted;
}

// XOR STRING DECRYPTION
String decryptMessage(const String& encrypted) {
    if (encrypted.length() == 0 || encrypted.length() % 2 != 0) { 
        return "";
    }
 
    String decrypted = "";
    int keyLength = strlen(ENCRYPTION_KEY);

    for (int i = 0; i < encrypted.length(); i += 2) {
        String byteHex = encrypted.substring(i, i + 2);
        byte encryptedByte = (byte)strtol(byteHex.c_str(), NULL, 16);
        
        char keyChar = ENCRYPTION_KEY[(i / 2) % keyLength];
        char decryptedChar = encryptedByte ^ keyChar;
        decrypted += decryptedChar;
    }
    
    return decrypted;
}

#endif