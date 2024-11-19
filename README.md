# BIOMETRIC WIFI CONFIG DOCUMENTATION

1. Communication message format → JSON
2. Use ‘*’ at the end of the each JSON payload to indicate the end of the message

Request JSON format

```json
{ "uid":"__USER_ID__", "ssid":"__WIFI_SSID__", "pwd":"__WIFI_PASSWORD__" }
```

Response formats

1. File system read operation error

```json
{ "est":"1", "ety":"1"}
```

1. Stored past JSON message decode error

```json
{ "est":"1", "ety":"2"}
```

1. Input JSON message decode error

```json
{ "est":"1", "ety":"3"}
```

1. Invalid user id

```json
{ "est":"1", "ety":"4"}
```

1. File system write operation error

```json
{ "est":"1", "ety":"5"}
```

1. WIFI configuration successfull

```json
{ "est":"0"}
```
