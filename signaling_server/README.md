# Certs
Place your `server.key` and `server.crt` files in the `signaling_server/certs/` directory. For testing, you can generate self-signed certificates (e.g., using OpenSSL), but for production, use certificates from a trusted Certificate Authority.

OpenSSL Self-Signed Cert Example:

```shell
mkdir signaling_server/certs
openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout signaling_server/certs/server.key -out signaling_server/certs/server.crt -subj "/CN=your_domain_or_ip"
```
