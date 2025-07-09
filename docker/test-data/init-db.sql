-- Create device_config table
CREATE TABLE IF NOT EXISTS device_config (
    device_id TEXT PRIMARY KEY,
    thing_name TEXT NOT NULL,
    iot_endpoint TEXT NOT NULL,
    aws_region TEXT NOT NULL,
    root_ca_path TEXT NOT NULL,
    certificate_pem TEXT NOT NULL,
    private_key_pem TEXT NOT NULL,
    role_alias TEXT NOT NULL,
    role_alias_endpoint TEXT NOT NULL,
    nucleus_version TEXT,
    deployment_group TEXT,
    initial_components TEXT,
    proxy_url TEXT,
    mqtt_port INTEGER,
    custom_domain TEXT
);

-- Create device_identifiers table
CREATE TABLE IF NOT EXISTS device_identifiers (
    device_id TEXT NOT NULL,
    mac_address TEXT,
    serial_number TEXT,
    FOREIGN KEY (device_id) REFERENCES device_config(device_id)
);

-- Insert test device configuration
INSERT INTO device_config (
    device_id,
    thing_name,
    iot_endpoint,
    aws_region,
    root_ca_path,
    certificate_pem,
    private_key_pem,
    role_alias,
    role_alias_endpoint,
    nucleus_version,
    deployment_group,
    initial_components
) VALUES (
    'test-device-001',
    'TestGreengrassThing',
    'mock-aws-iot:1080',
    'us-east-1',
    '-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----',
    '-----BEGIN CERTIFICATE-----
MIIDWjCCAkKgAwIBAgIVAJHKzkFgVEp8omQEH4MN0bqcZELLMA0GCSqGSIb3DQEB
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t
IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yNDAxMTUxMjAw
MDBaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh
dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC+QJzYXesENEbX8NCH
3JwBB2vVU9DV9JEY+h3yDm4KTXPXF8YPBPX8AGk8Q0eYoXGDPIS3F3sL9A7dDINO
W0BzGaNBB2vVU9DV9JEYA+h3yDm4KTXPXF8YPBPX8AGk8Q0eYoXGDPIS3F3sL9A7
dDINOW0BzGaNBdU7Aq7kMJOzQSUHmgr4oUfsQmmuQPvviIqx+BYC6qPF6vQ1E8w2
OZAQoUfsQmmuQPvviIqxcb5Mxi4Nc41TxqaqXjBwCGvQsYMjHlN8+s1SykoNh0bI
J2Gge2b8QNuBqP+O2Zy9AgMBAAGjYDBeMB8GA1UdIwQYMBaAFM1p6lBvZPl6yKcN
snMnvQR4qF+2MB0GA1UdDgQWBBROJqdTB5P2bShXW1WR4cMdosWFajAMBgNVHRMB
Af8EAjAAMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAjwCgPupr
HY+0cm7VHz+FUTgRxJs6brI8L4YDm1ReaJGdFupT1p3khYaYi5L8HGwN1oYJh0BH
YBJtfivPoKpFrT17Kp7qFYYLZQmmuN1T8hYH8agLUcKXwdaP8GN3YBwcfrKc+iJU
ohI8R8nVZyHqZAgDiZhYD3vE1hmMWIWGBKLBQKxMF2chhlWJQFF0XYU7xyMXy0rC
TbJGQw88fGwN1oYJh0BHI8R8nVZyHqZAgDiZhYD3vE1hhI8R8nVZyHqZAgDiZhYD
3vE1hmMWIWGBKLBQPJQFF0XYU7xyMXy0rCTbJGQw88f5oWl8FZhYD3vE1hmMWIWG
BKLBQKxMF2chhlWJQFF0XYU7xyMXy0rCTbJGQw88f5oWl8FZQ==
-----END CERTIFICATE-----',
    '-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAvkCc2F3rBDRG1/DQh9ycAQdr1VPQ1fSRGPod8g5uCk1z1xfG
DwT1/ABpPENHmKFxgzyEtxd7C/QO3QyDTltAcxmjQQdr1VPQ1fSRGAPod8g5uCk1
z1xfGDwT1/ABpPENHmKFxgzyEtxd7C/QO3QyDTltAcxmjQXVOwKu5DCTs0ElB5oK
+KFH7EJprkD774iKsfgWAuqjxer0NRPMNjmQEKFH7EJprkD774iKsXG+TMYuDXON
U8amql4wcAhr0LGDI4wcAhr0LGDIx5TfPrNUspKDYdGyCdhoHtm/EDbgaj/jtmcv
QIDAQABAoIBADK/tqJpYvYWyOEsHNqhWRMu4PIMIEkYMlkULU8EL8b5d9Y5FPrM
jfEeKPIdPBFfCbH3u7rJeC/nK0kGgAFRHP3pLFqMOGgT+fcrb1AqSbDcGS5msBLb
ESYnLNF2pzDvyZ5cR7duSv5Ou2hzPPSLLU8EL8b5d9Y5FPrMjfEeKPIdPBFfCbH3
u7rJeC/nK0kGgAFRHP3pLFqMOGgT+fcrb1AqSbDcGS5msBLbESYnLNF2pzDvyZ5c
R7duSv5Ou2hz9Y5FPrMjfEeKPIdPBFfCbH3u7rJeC/nK0kGgAFRHP3pLFqMOGgT+
fcrb1AqSbDcGS5msBLbESYnLNF2pzDvyZ5cR7duSv5Ou2hzPPSBECgYEA4vVt5J8O
u2hzPPSBEu2hzPPSBEu2hzPPSBE4wcAhr0LGDIx5TfPrNUspKDYdGyCdhoHtm/ED
bgaj/jtmcvQIDAQABu2hzPPSBECgYEA2vWu2hzPPSBE4wcAhr0LGDIx5TfPrNUsp
KDYdGyCdhoHtm/EDbgaj/jtmcvQu2hzPPSBE4wcAhr0LGDIx5TfPrNUspKDYdGyC
dhoHtm/EDbgaj/jtmcvQIDAQABu2hzPPQBCgYA4u2hzPPSBEvWu2hzPPSBEQBECgYE
A2vWLU8EL8b5d9Y5FPrMjfEeKPIdPBFfCbH3u7rJeC/nK0kGgAFRHP3pLFqMOGgT+
fcrb1AqSbDcGS5msBLbESYnLNF2pzDvyZ5cR7duSv5Ou2hzPPSBECgYBAJKejYu2
hzPPSBE4wcAhr0LGDIx5TfPrNUspKDYdGyCdhoHtm/EDlYXQdFWFO8cjF8tKwk2y
RkMPPH+aFpfBBE4wcAhr0LGDIx5Tf4wcAhr0LGDIx5TfPrNUsp9Y5FPrM6jfEeKP
IdPBFfCbH3u7rJeC/nK0kGgAFRHP3pLBRHP3pLyPwgP38CLBRHP3pL5YBRHP38=
-----END RSA PRIVATE KEY-----',
    'GreengrassV2TokenExchangeRole',
    'mock-aws-iot:1080',
    '2.9.0',
    'test-deployment-group',
    'aws.greengrass.Cli,aws.greengrass.LocalDebugConsole'
);

-- Insert device identifier mapping
INSERT INTO device_identifiers (device_id, mac_address, serial_number) VALUES 
    ('test-device-001', '02:42:ac:11:00:02', 'TEST-SERIAL-001'),
    ('test-device-001', 'aa:bb:cc:dd:ee:ff', 'TEST-SERIAL-002');

-- Insert a default device for fallback
INSERT INTO device_config (
    device_id,
    thing_name,
    iot_endpoint,
    aws_region,
    root_ca_path,
    certificate_pem,
    private_key_pem,
    role_alias,
    role_alias_endpoint,
    nucleus_version,
    deployment_group
) SELECT 
    'default',
    'DefaultGreengrassThing',
    iot_endpoint,
    aws_region,
    root_ca_path,
    certificate_pem,
    private_key_pem,
    role_alias,
    role_alias_endpoint,
    nucleus_version,
    'default-deployment-group'
FROM device_config WHERE device_id = 'test-device-001'; 