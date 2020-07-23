/*
    Secure Credentials
*/

#ifndef _SECURE_CREDENTIALS_H_
#define _SECURE_CREDENTIALS_H_

const char caCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
//paste the contents of your ca cert file here
-----END CERTIFICATE-----
)EOF";

/* openssl x509 -noout -in mqtt-serv.crt -fingerprint
sample: const uint8_t mqttCertFingerprint[] = {0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00};  */
const uint8_t mqttCertFingerprint[] = {};

#endif // _SECURE_CREDENTIALS_H_
