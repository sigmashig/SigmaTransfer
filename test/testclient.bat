rem echo {'clientId':'123','authKey':'secret-api-key-12345'} | websocat -v -t ws://192.168.0.214:8080
websocat -v -t ws://192.168.0.214:8080 < msg1.txt